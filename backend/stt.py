"""Local WAV validation, normalization, and NobodyWho transcription."""

from __future__ import annotations

from dataclasses import dataclass
import math
import os
from pathlib import Path
import struct
import tempfile
from typing import Any, Callable, Protocol, Sequence


class SpeechToTextError(RuntimeError):
    """Base class for user-actionable speech-to-text failures."""


class WavInputError(SpeechToTextError):
    """Raised when an input file is not a supported, complete WAV."""


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
    def transcribe_file(self, path: str) -> _TokenStream: ...

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


@dataclass(frozen=True, slots=True)
class _WavInfo:
    format_code: int
    channels: int
    sample_width: int
    sample_rate: int
    frame_count: int
    data_offset: int
    data_size: int
    block_align: int


_WAVE_FORMAT_PCM = 0x0001
_WAVE_FORMAT_IEEE_FLOAT = 0x0003
_WAVE_FORMAT_EXTENSIBLE = 0xFFFE
_EXTENSIBLE_GUID_TAIL = bytes.fromhex("00001000800000aa00389b71")


def _truncated_wav(path: Path) -> WavInputError:
    return WavInputError(
        f"WAV 오디오 데이터가 헤더에 기록된 길이보다 짧습니다: {path}"
    )


def _read_wav_info(path: Path) -> _WavInfo:
    try:
        file_size = path.stat().st_size
        wav_file = path.open("rb")
    except OSError as error:
        raise WavInputError(
            f"WAV 파일이 손상되었거나 읽을 수 없습니다: {path}"
        ) from error

    with wav_file:
        try:
            header = wav_file.read(12)
            if len(header) != 12:
                raise WavInputError(
                    f"WAV 파일이 손상되었거나 읽을 수 없습니다: {path}"
                )

            riff_id, riff_size, wave_id = struct.unpack("<4sI4s", header)
            if riff_id != b"RIFF" or wave_id != b"WAVE":
                raise WavInputError(
                    f"WAV 파일이 손상되었거나 읽을 수 없습니다: {path}"
                )
            if riff_size == 0xFFFFFFFF:
                raise WavInputError(
                    f"RF64 WAV는 지원하지 않습니다. RIFF WAV를 사용하세요: {path}"
                )

            riff_end = 8 + riff_size
            if riff_end > file_size:
                raise _truncated_wav(path)

            format_payload: bytes | None = None
            data_offset: int | None = None
            data_size: int | None = None
            cursor = 12
            while cursor < riff_end:
                if cursor + 8 > riff_end:
                    raise WavInputError(
                        f"WAV 청크 헤더가 올바르지 않습니다: {path}"
                    )
                wav_file.seek(cursor)
                chunk_header = wav_file.read(8)
                if len(chunk_header) != 8:
                    raise _truncated_wav(path)

                chunk_id, chunk_size = struct.unpack("<4sI", chunk_header)
                chunk_offset = cursor + 8
                chunk_end = chunk_offset + chunk_size
                padded_end = chunk_end + (chunk_size & 1)
                if chunk_end > riff_end or chunk_end > file_size:
                    raise _truncated_wav(path)

                if chunk_id == b"fmt " and format_payload is None:
                    if chunk_size > 65_536:
                        raise WavInputError(
                            f"WAV fmt 청크가 올바르지 않습니다: {path}"
                        )
                    wav_file.seek(chunk_offset)
                    format_payload = wav_file.read(chunk_size)
                    if len(format_payload) != chunk_size:
                        raise _truncated_wav(path)
                elif chunk_id == b"data" and data_offset is None:
                    data_offset = chunk_offset
                    data_size = chunk_size

                cursor = padded_end
        except (OSError, struct.error) as error:
            raise WavInputError(
                f"WAV 파일이 손상되었거나 읽을 수 없습니다: {path}"
            ) from error

    if format_payload is None or data_offset is None or data_size is None:
        raise WavInputError(f"WAV 필수 청크가 없습니다: {path}")
    if len(format_payload) < 16:
        raise WavInputError(f"WAV fmt 청크가 올바르지 않습니다: {path}")

    format_code, channels, sample_rate, byte_rate, block_align, bits_per_sample = (
        struct.unpack("<HHIIHH", format_payload[:16])
    )
    if format_code == _WAVE_FORMAT_EXTENSIBLE:
        if len(format_payload) < 40:
            raise WavInputError(
                f"WAVE_FORMAT_EXTENSIBLE fmt 청크가 올바르지 않습니다: {path}"
            )
        extension_size = struct.unpack("<H", format_payload[16:18])[0]
        subformat_guid = format_payload[24:40]
        if extension_size < 22 or subformat_guid[4:] != _EXTENSIBLE_GUID_TAIL:
            raise WavInputError(
                f"지원하지 않는 WAVE_FORMAT_EXTENSIBLE 형식입니다: {path}"
            )
        format_code = int.from_bytes(subformat_guid[:4], "little")

    if format_code not in {_WAVE_FORMAT_PCM, _WAVE_FORMAT_IEEE_FLOAT}:
        raise WavInputError(
            f"압축 WAV는 지원하지 않습니다. PCM 또는 IEEE float WAV를 사용하세요: {path}"
        )
    if channels < 1:
        raise WavInputError(f"WAV 채널 수가 올바르지 않습니다: {path}")
    if sample_rate < 1:
        raise WavInputError(f"WAV 샘플레이트가 올바르지 않습니다: {path}")
    if bits_per_sample % 8 != 0:
        raise WavInputError(
            f"WAV 샘플 폭이 바이트 단위가 아닙니다: {bits_per_sample}-bit"
        )

    sample_width = bits_per_sample // 8
    supported_widths = (
        {1, 2, 3, 4}
        if format_code == _WAVE_FORMAT_PCM
        else {4, 8}
    )
    if sample_width not in supported_widths:
        format_name = "PCM" if format_code == _WAVE_FORMAT_PCM else "IEEE float"
        raise WavInputError(
            f"지원하지 않는 WAV 샘플 폭입니다: {format_name} {bits_per_sample}-bit"
        )

    expected_block_align = channels * sample_width
    if block_align != expected_block_align or byte_rate != sample_rate * block_align:
        raise WavInputError(f"WAV 오디오 정렬 정보가 올바르지 않습니다: {path}")
    if data_size % block_align != 0:
        raise WavInputError(f"WAV data 청크 크기가 올바르지 않습니다: {path}")

    return _WavInfo(
        format_code=format_code,
        channels=channels,
        sample_width=sample_width,
        sample_rate=sample_rate,
        frame_count=data_size // block_align,
        data_offset=data_offset,
        data_size=data_size,
        block_align=block_align,
    )


def _iter_pcm16_samples(
    raw_frames: bytes, sample_width: int, format_code: int
):
    if format_code == _WAVE_FORMAT_IEEE_FLOAT:
        unpack_format = "<f" if sample_width == 4 else "<d"
        for (value,) in struct.iter_unpack(unpack_format, raw_frames):
            if not math.isfinite(value):
                raise WavInputError(
                    "IEEE float WAV에 NaN 또는 무한대 샘플이 포함되어 있습니다."
                )
            if value <= -1.0:
                yield -32768
            elif value >= 1.0:
                yield 32767
            else:
                yield int(round(value * 32767.0))
        return

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
    raw_frames: bytes,
    *,
    channels: int,
    sample_width: int,
    frame_count: int,
    format_code: int,
) -> tuple[list[int], int]:
    source_samples = iter(
        _iter_pcm16_samples(raw_frames, sample_width, format_code)
    )
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


def _wav_has_non_silent_audio(path: Path, info: _WavInfo) -> bool:
    if info.frame_count == 0:
        return False

    chunk_size = info.block_align * max(1, (1024 * 1024) // info.block_align)
    remaining = info.data_size
    try:
        with path.open("rb") as wav_file:
            wav_file.seek(info.data_offset)
            while remaining:
                block = wav_file.read(min(chunk_size, remaining))
                if not block:
                    raise _truncated_wav(path)
                remaining -= len(block)

                if info.format_code == _WAVE_FORMAT_PCM:
                    if info.sample_width == 1:
                        if block != bytes((128,)) * len(block):
                            return True
                    elif any(block):
                        return True
                    continue

                if any(block):
                    unpack_format = "<f" if info.sample_width == 4 else "<d"
                    for (value,) in struct.iter_unpack(unpack_format, block):
                        if not math.isfinite(value):
                            raise WavInputError(
                                "IEEE float WAV에 NaN 또는 무한대 샘플이 포함되어 있습니다."
                            )
                        if value != 0.0:
                            return True
    except OSError as error:
        raise WavInputError(f"WAV 파일을 읽을 수 없습니다: {path}") from error
    return False


def _audio_summary(info: _WavInfo, *, non_silent: bool) -> NormalizedAudio:
    return NormalizedAudio(
        samples=[],
        sample_rate=info.sample_rate,
        source_channels=info.channels,
        source_sample_width=info.sample_width,
        frame_count=info.frame_count,
        peak_amplitude=1 if non_silent else 0,
    )


def load_and_normalize_wav(path: Path) -> NormalizedAudio:
    """Validate an uncompressed WAV and convert it to mono signed 16-bit samples."""

    info = _read_wav_info(path)
    try:
        with path.open("rb") as wav_file:
            wav_file.seek(info.data_offset)
            raw_frames = wav_file.read(info.data_size)
    except OSError as error:
        raise WavInputError(f"WAV 파일을 읽을 수 없습니다: {path}") from error
    if len(raw_frames) != info.data_size:
        raise _truncated_wav(path)

    samples, peak_amplitude = _downmix_to_mono(
        raw_frames,
        channels=info.channels,
        sample_width=info.sample_width,
        frame_count=info.frame_count,
        format_code=info.format_code,
    )
    return NormalizedAudio(
        samples=samples,
        sample_rate=info.sample_rate,
        source_channels=info.channels,
        source_sample_width=info.sample_width,
        frame_count=info.frame_count,
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

    wav_info = _read_wav_info(input_path)
    non_silent = _wav_has_non_silent_audio(input_path, wav_info)
    audio = _audio_summary(wav_info, non_silent=non_silent)
    if not non_silent:
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
        transcribe_file = getattr(engine, "transcribe_file", None)
        if callable(transcribe_file):
            token_stream = transcribe_file(str(input_path))
        else:
            audio = load_and_normalize_wav(input_path)
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
