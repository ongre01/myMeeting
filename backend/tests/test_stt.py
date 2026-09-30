from __future__ import annotations

from pathlib import Path
import struct
import tempfile
import unittest
from unittest import mock
import wave

from backend.stt import (
    ModelLoadError,
    TranscriptWriteError,
    TranscriptionError,
    WavInputError,
    load_and_normalize_wav,
    transcribe_wav,
    write_transcript,
)


class _CompletedStream:
    def __init__(self, text: object) -> None:
        self._text = text

    def completed(self) -> object:
        return self._text


class _RecordingEngine:
    def __init__(self, calls: list[tuple[list[int], int]], text: object) -> None:
        self._calls = calls
        self._text = text

    def transcribe_pcm(self, samples: list[int], sample_rate: int):
        self._calls.append((samples, sample_rate))
        return _CompletedStream(self._text)


class SpeechToTextTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary_directory.cleanup)
        self.root = Path(self.temporary_directory.name)
        self.input_path = self.root / "meeting.wav"
        self.transcript_path = self.root / "transcript.txt"

    @staticmethod
    def _write_wav(
        path: Path,
        raw_frames: bytes,
        *,
        channels: int = 1,
        sample_width: int = 2,
        sample_rate: int = 16_000,
    ) -> None:
        with wave.open(str(path), "wb") as wav_file:
            wav_file.setnchannels(channels)
            wav_file.setsampwidth(sample_width)
            wav_file.setframerate(sample_rate)
            wav_file.writeframes(raw_frames)

    def test_pcm_sample_widths_are_normalized_to_signed_16_bit(self) -> None:
        cases = (
            (1, bytes((0, 128, 255)), [-32768, 0, 32512]),
            (2, struct.pack("<hhh", -32768, 0, 32767), [-32768, 0, 32767]),
            (
                3,
                b"\x00\x00\x80\x00\x00\x00\x00\xff\x7f",
                [-32768, 0, 32767],
            ),
            (
                4,
                struct.pack("<iii", -2147483648, 0, 2147418112),
                [-32768, 0, 32767],
            ),
        )

        for sample_width, frames, expected in cases:
            with self.subTest(sample_width=sample_width):
                path = self.root / f"pcm-{sample_width}.wav"
                self._write_wav(path, frames, sample_width=sample_width)

                audio = load_and_normalize_wav(path)

                self.assertEqual(audio.samples, expected)
                self.assertEqual(audio.source_sample_width, sample_width)

    def test_stereo_is_downmixed_and_original_sample_rate_is_forwarded(self) -> None:
        self._write_wav(
            self.input_path,
            struct.pack("<hhhh", 10_000, -2_000, -10_000, 2_000),
            channels=2,
            sample_rate=48_000,
        )
        factory_calls: list[tuple[str, str]] = []
        inference_calls: list[tuple[list[int], int]] = []

        def factory(*, source: str, language: str):
            factory_calls.append((source, language))
            return _RecordingEngine(inference_calls, "  오늘 회의를 시작합니다.  ")

        result = transcribe_wav(
            self.input_path,
            model_source="local-whisper",
            language="ko",
            engine_factory=factory,
        )

        self.assertEqual(result.text, "오늘 회의를 시작합니다.")
        self.assertFalse(result.skipped_silence)
        self.assertEqual(factory_calls, [("local-whisper", "ko")])
        self.assertEqual(inference_calls, [([4_000, -4_000], 48_000)])

    def test_one_frame_non_silent_input_is_transcribed(self) -> None:
        self._write_wav(self.input_path, struct.pack("<h", 1))
        calls: list[tuple[list[int], int]] = []

        result = transcribe_wav(
            self.input_path,
            model_source="local-whisper",
            language="ko",
            engine_factory=lambda **_: _RecordingEngine(calls, "짧은 발화"),
        )

        self.assertEqual(result.text, "짧은 발화")
        self.assertEqual(calls, [([1], 16_000)])

    def test_regional_language_tag_is_normalized_for_whisper(self) -> None:
        self._write_wav(self.input_path, struct.pack("<h", 1))
        factory_calls: list[tuple[str, str]] = []

        def factory(*, source: str, language: str):
            factory_calls.append((source, language))
            return _RecordingEngine([], "한국어")

        transcribe_wav(
            self.input_path,
            model_source="local-whisper",
            language="KO-kr",
            engine_factory=factory,
        )

        self.assertEqual(factory_calls, [("local-whisper", "ko")])

    def test_empty_and_silent_input_skip_model_loading(self) -> None:
        for name, frames in (
            ("empty.wav", b""),
            ("silent.wav", struct.pack("<hhhh", 0, 0, 0, 0)),
        ):
            with self.subTest(name=name):
                path = self.root / name
                self._write_wav(path, frames)

                def unexpected_factory(**_: object):
                    self.fail("The model must not load for silent audio")

                result = transcribe_wav(
                    path,
                    model_source="local-whisper",
                    language="ko",
                    engine_factory=unexpected_factory,
                )

                self.assertEqual(result.text, "")
                self.assertTrue(result.skipped_silence)

    def test_corrupt_wav_is_rejected(self) -> None:
        self.input_path.write_bytes(b"not a wav")

        with self.assertRaisesRegex(WavInputError, "손상"):
            load_and_normalize_wav(self.input_path)

    def test_truncated_wav_data_is_rejected(self) -> None:
        self._write_wav(self.input_path, struct.pack("<hhhh", 1, 2, 3, 4))
        self.input_path.write_bytes(self.input_path.read_bytes()[:-1])

        with self.assertRaisesRegex(WavInputError, "헤더에 기록된 길이"):
            load_and_normalize_wav(self.input_path)

    def test_model_loading_error_is_wrapped(self) -> None:
        self._write_wav(self.input_path, struct.pack("<h", 1))

        def failing_factory(**_: object):
            raise RuntimeError("model unavailable")

        with self.assertRaisesRegex(ModelLoadError, "local-whisper"):
            transcribe_wav(
                self.input_path,
                model_source="local-whisper",
                language="ko",
                engine_factory=failing_factory,
            )

    def test_inference_error_and_non_text_result_are_rejected(self) -> None:
        self._write_wav(self.input_path, struct.pack("<h", 1))

        class FailingEngine:
            def transcribe_pcm(self, samples: list[int], sample_rate: int):
                raise RuntimeError("inference failed")

        with self.assertRaisesRegex(TranscriptionError, "음성 인식에 실패"):
            transcribe_wav(
                self.input_path,
                model_source="local-whisper",
                language="ko",
                engine_factory=lambda **_: FailingEngine(),
            )

        with self.assertRaisesRegex(TranscriptionError, "문자열이 아닙니다"):
            transcribe_wav(
                self.input_path,
                model_source="local-whisper",
                language="ko",
                engine_factory=lambda **_: _RecordingEngine([], None),
            )

    def test_transcript_is_utf8_and_ends_with_one_newline(self) -> None:
        write_transcript(self.transcript_path, "회의 시작\n\n")

        self.assertEqual(self.transcript_path.read_bytes(), "회의 시작\n".encode("utf-8"))

    def test_atomic_write_failure_preserves_existing_transcript(self) -> None:
        self.transcript_path.write_text("기존 내용\n", encoding="utf-8")

        with mock.patch("backend.stt.os.replace", side_effect=PermissionError("denied")):
            with self.assertRaises(TranscriptWriteError):
                write_transcript(self.transcript_path, "새 내용")

        self.assertEqual(
            self.transcript_path.read_text(encoding="utf-8"), "기존 내용\n"
        )
        self.assertEqual(list(self.root.glob(".transcript.txt.*.tmp")), [])


if __name__ == "__main__":
    unittest.main()
