"""Command-line entry point for the local myMeeting backend."""

from __future__ import annotations

import argparse
from enum import IntEnum
import json
from pathlib import Path
import sys
import tempfile
from typing import Sequence

if __package__ in {None, ""}:
    sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from backend.configuration import ConfigurationError, ModelConfiguration
from backend.configuration import load_model_configuration
from backend.summarizer import (
    LlmDependencyError,
    LlmInferenceError,
    LlmModelLoadError,
    LlmResponseValidationError,
    MinutesWriteError,
    TranscriptInputError,
    analyze_transcript,
    read_transcript,
    write_meeting_minutes,
)
from backend.stt import (
    ModelLoadError,
    SttDependencyError,
    TranscriptWriteError,
    TranscriptionError,
    WavInputError,
    transcribe_wav,
    write_transcript,
)


MINIMUM_PYTHON_VERSION = (3, 11)


class ExitCode(IntEnum):
    """Stable process exit codes consumed by the future Qt client."""

    SUCCESS = 0
    USAGE_ERROR = 2
    INPUT_ERROR = 3
    OUTPUT_ERROR = 4
    CONFIGURATION_ERROR = 5
    RUNTIME_ERROR = 6


class CliValidationError(ValueError):
    """A user-actionable validation failure with a stable exit code."""

    def __init__(self, exit_code: ExitCode, message: str) -> None:
        super().__init__(message)
        self.exit_code = exit_code


def default_config_path() -> Path:
    return Path(__file__).resolve().with_name("config.json")


def build_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "NobodyWho로 로컬 WAV 또는 편집된 Transcript를 분석해 "
            "meeting.json을 생성합니다."
        )
    )
    input_group = parser.add_mutually_exclusive_group(required=True)
    input_group.add_argument("--input", type=Path, help="입력 WAV 경로")
    input_group.add_argument(
        "--transcript-input", type=Path, help="분석할 편집된 Transcript TXT 경로"
    )
    parser.add_argument("--output", required=True, type=Path, help="출력 JSON 경로")
    parser.add_argument(
        "--config",
        type=Path,
        default=default_config_path(),
        help="모델 설정 JSON 경로 (기본값: backend/config.json)",
    )
    parser.add_argument(
        "--transcript-output",
        type=Path,
        help="Transcript TXT 경로 (기본값: 출력 JSON 폴더의 transcript.txt)",
    )
    parser.add_argument(
        "--progress",
        action="store_true",
        help="Qt 클라이언트용 JSON Lines 진행 상태를 표준 출력에 기록합니다.",
    )
    return parser


def _absolute_path(path: Path, label: str, exit_code: ExitCode) -> Path:
    try:
        return path.expanduser().resolve(strict=False)
    except (OSError, RuntimeError) as error:
        raise CliValidationError(
            exit_code, f"{label} 경로를 해석할 수 없습니다: {path} ({error})"
        ) from error


def validate_input_path(path: Path) -> Path:
    input_path = _absolute_path(path, "입력", ExitCode.INPUT_ERROR)
    if input_path.suffix.lower() != ".wav":
        raise CliValidationError(
            ExitCode.INPUT_ERROR, f"입력 파일은 WAV 형식이어야 합니다: {input_path}"
        )
    if not input_path.exists():
        raise CliValidationError(
            ExitCode.INPUT_ERROR, f"입력 WAV 파일을 찾을 수 없습니다: {input_path}"
        )
    if not input_path.is_file():
        raise CliValidationError(
            ExitCode.INPUT_ERROR, f"입력 WAV 경로가 파일이 아닙니다: {input_path}"
        )

    try:
        with input_path.open("rb") as input_file:
            input_file.read(0)
    except OSError as error:
        raise CliValidationError(
            ExitCode.INPUT_ERROR,
            f"입력 WAV 파일을 읽을 수 없습니다: {input_path} ({error})",
        ) from error

    return input_path


def _verify_output_directory(parent: Path) -> None:
    try:
        with tempfile.NamedTemporaryFile(
            mode="wb", prefix=".mymeeting-write-", dir=parent, delete=True
        ):
            pass
    except OSError as error:
        raise CliValidationError(
            ExitCode.OUTPUT_ERROR,
            f"출력 폴더에 파일을 쓸 수 없습니다: {parent} ({error})",
        ) from error


def validate_output_path(path: Path) -> Path:
    output_path = _absolute_path(path, "출력", ExitCode.OUTPUT_ERROR)
    if output_path.suffix.lower() != ".json":
        raise CliValidationError(
            ExitCode.OUTPUT_ERROR,
            f"출력 파일은 JSON 형식이어야 합니다: {output_path}",
        )

    parent = output_path.parent
    if not parent.exists():
        raise CliValidationError(
            ExitCode.OUTPUT_ERROR, f"출력 폴더를 찾을 수 없습니다: {parent}"
        )
    if not parent.is_dir():
        raise CliValidationError(
            ExitCode.OUTPUT_ERROR, f"출력 상위 경로가 폴더가 아닙니다: {parent}"
        )
    if output_path.exists() and not output_path.is_file():
        raise CliValidationError(
            ExitCode.OUTPUT_ERROR, f"출력 경로가 파일이 아닙니다: {output_path}"
        )
    if output_path.exists():
        try:
            with output_path.open("r+b"):
                pass
        except OSError as error:
            raise CliValidationError(
                ExitCode.OUTPUT_ERROR,
                f"출력 파일에 쓸 수 없습니다: {output_path} ({error})",
            ) from error

    _verify_output_directory(parent)
    return output_path


def validate_transcript_output_path(path: Path) -> Path:
    transcript_path = _absolute_path(path, "Transcript 출력", ExitCode.OUTPUT_ERROR)
    if transcript_path.suffix.lower() != ".txt":
        raise CliValidationError(
            ExitCode.OUTPUT_ERROR,
            f"Transcript 출력 파일은 TXT 형식이어야 합니다: {transcript_path}",
        )

    parent = transcript_path.parent
    if not parent.exists():
        raise CliValidationError(
            ExitCode.OUTPUT_ERROR, f"Transcript 출력 폴더를 찾을 수 없습니다: {parent}"
        )
    if not parent.is_dir():
        raise CliValidationError(
            ExitCode.OUTPUT_ERROR,
            f"Transcript 출력 상위 경로가 폴더가 아닙니다: {parent}",
        )
    if transcript_path.exists() and not transcript_path.is_file():
        raise CliValidationError(
            ExitCode.OUTPUT_ERROR,
            f"Transcript 출력 경로가 파일이 아닙니다: {transcript_path}",
        )
    if transcript_path.exists():
        try:
            with transcript_path.open("r+b"):
                pass
        except OSError as error:
            raise CliValidationError(
                ExitCode.OUTPUT_ERROR,
                f"Transcript 출력 파일에 쓸 수 없습니다: {transcript_path} ({error})",
            ) from error

    _verify_output_directory(parent)
    return transcript_path


def validate_transcript_input_path(path: Path) -> Path:
    transcript_path = _absolute_path(path, "Transcript 입력", ExitCode.INPUT_ERROR)
    if transcript_path.suffix.lower() != ".txt":
        raise CliValidationError(
            ExitCode.INPUT_ERROR,
            f"Transcript 입력 파일은 TXT 형식이어야 합니다: {transcript_path}",
        )
    if not transcript_path.exists():
        raise CliValidationError(
            ExitCode.INPUT_ERROR,
            f"Transcript 입력 파일을 찾을 수 없습니다: {transcript_path}",
        )
    if not transcript_path.is_file():
        raise CliValidationError(
            ExitCode.INPUT_ERROR,
            f"Transcript 입력 경로가 파일이 아닙니다: {transcript_path}",
        )
    try:
        with transcript_path.open("rb") as transcript_file:
            transcript_file.read(0)
    except OSError as error:
        raise CliValidationError(
            ExitCode.INPUT_ERROR,
            f"Transcript 입력 파일을 읽을 수 없습니다: {transcript_path} ({error})",
        ) from error
    return transcript_path


def validate_configuration_path(path: Path) -> tuple[Path, ModelConfiguration]:
    config_path = _absolute_path(path, "설정", ExitCode.CONFIGURATION_ERROR)
    try:
        configuration = load_model_configuration(config_path)
    except ConfigurationError as error:
        raise CliValidationError(ExitCode.CONFIGURATION_ERROR, str(error)) from error
    return config_path, configuration


def python_version_is_supported(version: Sequence[int] = sys.version_info) -> bool:
    return tuple(version[:2]) >= MINIMUM_PYTHON_VERSION


def _configure_standard_streams() -> None:
    for stream in (sys.stdout, sys.stderr):
        reconfigure = getattr(stream, "reconfigure", None)
        if reconfigure is not None:
            reconfigure(encoding="utf-8", errors="replace")


def _emit_progress(enabled: bool, status: str) -> None:
    if enabled:
        print(json.dumps({"status": status}, ensure_ascii=False), flush=True)


def main(argv: Sequence[str] | None = None) -> int:
    _configure_standard_streams()

    if not python_version_is_supported():
        required = ".".join(map(str, MINIMUM_PYTHON_VERSION))
        current = f"{sys.version_info.major}.{sys.version_info.minor}"
        print(
            f"오류: Python {required} 이상이 필요합니다. 현재 버전: {current}",
            file=sys.stderr,
        )
        return int(ExitCode.RUNTIME_ERROR)

    arguments = build_argument_parser().parse_args(argv)

    try:
        output_path = validate_output_path(arguments.output)
        config_path, configuration = validate_configuration_path(arguments.config)
        if arguments.input is not None:
            input_path = validate_input_path(arguments.input)
            requested_transcript_path = (
                arguments.transcript_output
                if arguments.transcript_output is not None
                else output_path.with_name("transcript.txt")
            )
            transcript_path = validate_transcript_output_path(
                requested_transcript_path
            )
            transcript_input_path = None
        else:
            if arguments.transcript_output is not None:
                raise CliValidationError(
                    ExitCode.USAGE_ERROR,
                    "--transcript-output은 --input WAV와 함께만 사용할 수 있습니다.",
                )
            input_path = None
            transcript_path = None
            transcript_input_path = validate_transcript_input_path(
                arguments.transcript_input
            )
    except CliValidationError as error:
        print(f"오류: {error}", file=sys.stderr)
        return int(error.exit_code)

    if input_path is not None:
        _emit_progress(arguments.progress, "transcribing")
        try:
            transcription = transcribe_wav(
                input_path,
                model_source=configuration.stt_model,
                language=configuration.language,
            )
            assert transcript_path is not None
            write_transcript(transcript_path, transcription.text)
        except WavInputError as error:
            print(f"오류: {error}", file=sys.stderr)
            return int(ExitCode.INPUT_ERROR)
        except TranscriptWriteError as error:
            print(f"오류: {error}", file=sys.stderr)
            return int(ExitCode.OUTPUT_ERROR)
        except (SttDependencyError, ModelLoadError, TranscriptionError) as error:
            print(f"오류: {error}", file=sys.stderr)
            return int(ExitCode.RUNTIME_ERROR)

        transcript = transcription.text
        if not transcript.strip():
            result = {
                "status": "transcribed",
                "input": str(input_path),
                "output": str(output_path),
                "transcript_output": str(transcript_path),
                "config": str(config_path),
                "language": configuration.language,
                "characters": 0,
                "audio_duration_seconds": round(
                    transcription.audio.duration_seconds, 3
                ),
                "silence": transcription.skipped_silence,
            }
            print(json.dumps(result, ensure_ascii=False))
            return int(ExitCode.SUCCESS)
    else:
        assert transcript_input_path is not None
        try:
            transcript = read_transcript(transcript_input_path)
        except TranscriptInputError as error:
            print(f"오류: {error}", file=sys.stderr)
            return int(ExitCode.INPUT_ERROR)

    try:
        _emit_progress(arguments.progress, "analyzing")
        minutes = analyze_transcript(
            transcript,
            model_source=configuration.llm_model,
        )
        write_meeting_minutes(output_path, minutes)
    except TranscriptInputError as error:
        print(f"오류: {error}", file=sys.stderr)
        return int(ExitCode.INPUT_ERROR)
    except MinutesWriteError as error:
        print(f"오류: {error}", file=sys.stderr)
        return int(ExitCode.OUTPUT_ERROR)
    except (
        LlmDependencyError,
        LlmModelLoadError,
        LlmInferenceError,
        LlmResponseValidationError,
    ) as error:
        print(f"오류: {error}", file=sys.stderr)
        return int(ExitCode.RUNTIME_ERROR)

    result = {
        "status": "completed",
        "output": str(output_path),
        "config": str(config_path),
        "language": configuration.language,
        "characters": len(transcript),
    }
    if input_path is not None:
        result.update(
            {
                "input": str(input_path),
                "transcript_output": str(transcript_path),
                "audio_duration_seconds": round(
                    transcription.audio.duration_seconds, 3
                ),
                "silence": transcription.skipped_silence,
            }
        )
    else:
        result["transcript_input"] = str(transcript_input_path)
    print(json.dumps(result, ensure_ascii=False))
    return int(ExitCode.SUCCESS)


if __name__ == "__main__":
    raise SystemExit(main())
