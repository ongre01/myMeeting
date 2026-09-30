"""Local WAV validation, normalization, and NobodyWho transcription."""

from __future__ import annotations

from dataclasses import dataclass
import os
from pathlib import Path
import tempfile
from typing import Any, Callable, Protocol, Sequence
import wave


class SpeechToTextError(RuntimeError):
    """Base class for user-actionable speech-to-text failures."""


class WavInputError(SpeechToTextError):
    """Raised when an input file is not a supported, complete PCM WAV."""


class SttDependencyError(SpeechToTextError):
    """Raised when the local NobodyWho runtime cannot be imported."""


class ModelLoadError(SpeechToTextError):
    """Raised when the configured local Whisper model cannot be loaded."""


class TranscriptionError(SpeechToTextError):
    """Raised when Whisper inference fails or returns an invalid result."""


class TranscriptWriteError(SpeechToTextError):
    """Raised when transcript.txt cannot be written safely."""


class _TokenStream(Protocol):
    def completed(self) -> str: ...


class _SpeechToTextEngine(Protocol):
    def transcribe_pcm(
        self, samples: Sequence[int], sample_rate: int
    ) -> _TokenStream: ...


SpeechToTextFactory = Callable[..., _SpeechToTextEngine]


@dataclass(frozen=True, slots=True)
class NormalizedAudio:
    """Mono signed 16-bit PCM prepared for NobodyWho."""

    samples: list[int]
    sample_rate: int
    source_channels: int
    source_sample_width: int
    frame_count: int
    peak_amplitude: int

    @property
    def duration_seconds(self) -> float:
        return self.frame_count / self.sample_rate

    @property
    def is_silent(self) -> bool:
        return self.peak_amplitude == 0


@dataclass(frozen=True, slots=True)
class TranscriptionResult:
    text: str
    audio: NormalizedAudio
    skipped_silence: bool


def _iter_pcm16_samples(raw_frames: bytes, sample_width: int):
    if sample_width == 1:
        for value in raw_frames:
            yield (value - 128) << 8
        return

    if sample_width == 2:
        for offset in range(0, len(raw_frames), 2):
            yield int.from_bytes(
                raw_frames[offset : offset + 2], "little", signed=True
            )
        return

    if sample_width == 3:
        for offset in range(0, len(raw_frames), 3):
            value = (
                raw_frames[offset]
                | (raw_frames[offset + 1] << 8)
                | (raw_frames[offset + 2] << 16)
            )
            if value & 0x800000:
                value -= 1 << 24
            yield value >> 8
        return

    if sample_width == 4:
        for offset in range(0, len(raw_frames), 4):
            value = int.from_bytes(
                raw_frames[offset : offset + 4], "little", signed=True
            )
            yield value >> 16
        return

    raise WavInputError(
        "지원하지 않는 WAV 샘플 폭입니다. PCM 8/16/24/32-bit만 지원합니다: "
        f"{sample_width * 8}-bit"
    )


def _downmix_to_mono(
    raw_frames: bytes, *, channels: int, sample_width: int, frame_count: int
) -> tuple[list[int], int]:
    source_samples = iter(_iter_pcm16_samples(raw_frames, sample_width))
    mono_samples: list[int] = []
    peak_amplitude = 0

    for _ in range(frame_count):
        total = sum(next(source_samples) for _ in range(channels))
        if total < 0:
            sample = -((-total) // channels)
        else:
            sample = total // channels
        mono_samples.append(sample)
        peak_amplitude = max(peak_amplitude, abs(sample))

    return mono_samples, peak_amplitude


def load_and_normalize_wav(path: Path) -> NormalizedAudio:
    """Validate a PCM WAV and convert it to mono signed 16-bit samples."""

    try:
        with wave.open(str(path), "rb") as wav_file:
            channels = wav_file.getnchannels()
            sample_width = wav_file.getsampwidth()
            sample_rate = wav_file.getframerate()
            frame_count = wav_file.getnframes()
            compression = wav_file.getcomptype()
            raw_frames = wav_file.readframes(frame_count)
    except (EOFError, OSError, wave.Error) as error:
        raise WavInputError(f"WAV 파일이 손상되었거나 읽을 수 없습니다: {path}") from error

    if compression != "NONE":
        raise WavInputError(
            f"압축 WAV는 지원하지 않습니다. PCM WAV를 사용하세요: {path}"
        )
    if channels < 1:
        raise WavInputError(f"WAV 채널 수가 올바르지 않습니다: {path}")
    if sample_rate < 1:
        raise WavInputError(f"WAV 샘플레이트가 올바르지 않습니다: {path}")
    if sample_width not in {1, 2, 3, 4}:
        raise WavInputError(
            "지원하지 않는 WAV 샘플 폭입니다. PCM 8/16/24/32-bit만 지원합니다: "
            f"{sample_width * 8}-bit"
        )

    expected_size = frame_count * channels * sample_width
    if len(raw_frames) != expected_size:
        raise WavInputError(
            "WAV 오디오 데이터가 헤더에 기록된 길이보다 짧습니다: "
            f"{path}"
        )

    samples, peak_amplitude = _downmix_to_mono(
        raw_frames,
        channels=channels,
        sample_width=sample_width,
        frame_count=frame_count,
    )
    return NormalizedAudio(
        samples=samples,
        sample_rate=sample_rate,
        source_channels=channels,
        source_sample_width=sample_width,
        frame_count=frame_count,
        peak_amplitude=peak_amplitude,
    )


def _create_nobodywho_engine(*, source: str, language: str) -> _SpeechToTextEngine:
    try:
        from nobodywho import SpeechToText
    except ImportError as error:
        raise SttDependencyError(
            "NobodyWho를 불러올 수 없습니다. "
            "backend/requirements.txt의 패키지를 설치하세요."
        ) from error

    return SpeechToText(source=source, language=language)


def transcribe_wav(
    input_path: Path,
    *,
    model_source: str,
    language: str,
    engine_factory: SpeechToTextFactory | None = None,
) -> TranscriptionResult:
    """Transcribe one local WAV without sending its audio to an external API."""

    audio = load_and_normalize_wav(input_path)
    if audio.is_silent:
        return TranscriptionResult(text="", audio=audio, skipped_silence=True)

    factory = engine_factory or _create_nobodywho_engine
    whisper_language = language.split("-", maxsplit=1)[0].lower()
    try:
        engine = factory(source=model_source, language=whisper_language)
    except SpeechToTextError:
        raise
    except Exception as error:
        raise ModelLoadError(
            f"Whisper 모델을 불러오지 못했습니다: {model_source} ({error})"
        ) from error

    try:
        token_stream = engine.transcribe_pcm(audio.samples, audio.sample_rate)
        text: Any = token_stream.completed()
    except Exception as error:
        raise TranscriptionError(f"Whisper 음성 인식에 실패했습니다: {error}") from error

    if not isinstance(text, str):
        raise TranscriptionError("Whisper 음성 인식 결과가 문자열이 아닙니다.")

    return TranscriptionResult(
        text=text.strip(), audio=audio, skipped_silence=False
    )


def write_transcript(path: Path, text: str) -> None:
    """Atomically save UTF-8 transcript text, preserving an old file on failure."""

    contents = f"{text.rstrip()}\n" if text.strip() else ""
    temporary_path: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w",
            encoding="utf-8",
            newline="\n",
            prefix=f".{path.name}.",
            suffix=".tmp",
            dir=path.parent,
            delete=False,
        ) as temporary_file:
            temporary_path = Path(temporary_file.name)
            temporary_file.write(contents)
        os.replace(temporary_path, path)
    except (OSError, UnicodeError) as error:
        if temporary_path is not None:
            try:
                temporary_path.unlink(missing_ok=True)
            except OSError:
                pass
        raise TranscriptWriteError(
            f"Transcript 파일을 저장할 수 없습니다: {path} ({error})"
        ) from error
