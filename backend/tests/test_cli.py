from __future__ import annotations

import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile
import unittest
import wave

from backend.main import ExitCode, python_version_is_supported


BACKEND_DIRECTORY = Path(__file__).resolve().parents[1]
MAIN_SCRIPT = BACKEND_DIRECTORY / "main.py"


class BackendCliTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary_directory.cleanup)
        self.root = Path(self.temporary_directory.name)
        self.input_path = self.root / "meeting.wav"
        self.output_path = self.root / "meeting.json"
        self.config_path = self.root / "config.json"
        self._write_empty_wav(self.input_path)
        self._write_config()

    @staticmethod
    def _write_empty_wav(path: Path) -> None:
        with wave.open(str(path), "wb") as wav_file:
            wav_file.setnchannels(1)
            wav_file.setsampwidth(2)
            wav_file.setframerate(16_000)

    def _write_config(self, **overrides: object) -> None:
        settings: dict[str, object] = {
            "stt_model": "local-whisper-model",
            "llm_model": "local-llm-model",
            "language": "ko",
        }
        settings.update(overrides)
        self.config_path.write_text(
            json.dumps(settings, ensure_ascii=False), encoding="utf-8"
        )

    def _run_cli(
        self,
        *,
        input_path: Path | None = None,
        transcript_input_path: Path | None = None,
        output_path: Path | None = None,
        transcript_output_path: Path | None = None,
        config_path: Path | None = None,
        environment: dict[str, str] | None = None,
        progress: bool = False,
    ) -> subprocess.CompletedProcess[str]:
        command = [
            sys.executable,
            str(MAIN_SCRIPT),
            "--output",
            str(output_path or self.output_path),
            "--config",
            str(config_path or self.config_path),
        ]
        if transcript_input_path is not None:
            command.extend(["--transcript-input", str(transcript_input_path)])
        else:
            command.extend(["--input", str(input_path or self.input_path)])
        if transcript_output_path is not None:
            command.extend(["--transcript-output", str(transcript_output_path)])
        if progress:
            command.append("--progress")
        return subprocess.run(
            command,
            capture_output=True,
            text=True,
            encoding="utf-8",
            env=environment,
            check=False,
        )

    def test_valid_paths_and_local_model_settings_succeed(self) -> None:
        environment = os.environ.copy()
        environment["OPENAI_API_KEY"] = "must-not-be-used"

        completed = self._run_cli(environment=environment)

        self.assertEqual(completed.returncode, ExitCode.SUCCESS, completed.stderr)
        response = json.loads(completed.stdout)
        self.assertEqual(response["status"], "transcribed")
        self.assertEqual(Path(response["input"]), self.input_path.resolve())
        self.assertEqual(Path(response["output"]), self.output_path.resolve())
        self.assertEqual(response["language"], "ko")
        self.assertTrue(response["silence"])
        self.assertEqual(response["characters"], 0)
        transcript_path = self.root / "transcript.txt"
        self.assertEqual(Path(response["transcript_output"]), transcript_path)
        self.assertEqual(transcript_path.read_bytes(), b"")
        self.assertFalse(self.output_path.exists())

    def test_progress_mode_emits_json_lines_without_changing_final_result(self) -> None:
        completed = self._run_cli(progress=True)

        self.assertEqual(completed.returncode, ExitCode.SUCCESS, completed.stderr)
        events = [json.loads(line) for line in completed.stdout.splitlines()]
        self.assertEqual(events[0], {"status": "transcribing"})
        self.assertEqual(events[-1]["status"], "transcribed")
        self.assertEqual(events[-1]["characters"], 0)

    def test_default_config_is_relative_to_backend_not_working_directory(self) -> None:
        completed = subprocess.run(
            [
                sys.executable,
                str(MAIN_SCRIPT),
                "--input",
                str(self.input_path),
                "--output",
                str(self.output_path),
            ],
            cwd=self.root,
            capture_output=True,
            text=True,
            encoding="utf-8",
            check=False,
        )

        self.assertEqual(completed.returncode, ExitCode.SUCCESS, completed.stderr)
        response = json.loads(completed.stdout)
        self.assertEqual(Path(response["config"]), BACKEND_DIRECTORY / "config.json")

    def test_missing_wav_returns_input_error(self) -> None:
        completed = self._run_cli(input_path=self.root / "missing.wav")

        self.assertEqual(completed.returncode, ExitCode.INPUT_ERROR)
        self.assertIn("입력 WAV 파일을 찾을 수 없습니다", completed.stderr)

    def test_non_wav_input_returns_input_error(self) -> None:
        text_input = self.root / "meeting.txt"
        text_input.write_text("not audio", encoding="utf-8")

        completed = self._run_cli(input_path=text_input)

        self.assertEqual(completed.returncode, ExitCode.INPUT_ERROR)
        self.assertIn("WAV 형식", completed.stderr)

    def test_corrupt_wav_returns_input_error_without_traceback(self) -> None:
        self.input_path.write_bytes(b"not a wav")

        completed = self._run_cli()

        self.assertEqual(completed.returncode, ExitCode.INPUT_ERROR)
        self.assertIn("손상", completed.stderr)
        self.assertNotIn("Traceback", completed.stderr)

    def test_missing_output_directory_returns_output_error(self) -> None:
        completed = self._run_cli(
            output_path=self.root / "missing-directory" / "meeting.json"
        )

        self.assertEqual(completed.returncode, ExitCode.OUTPUT_ERROR)
        self.assertIn("출력 폴더를 찾을 수 없습니다", completed.stderr)

    def test_non_json_output_returns_output_error(self) -> None:
        completed = self._run_cli(output_path=self.root / "meeting.txt")

        self.assertEqual(completed.returncode, ExitCode.OUTPUT_ERROR)
        self.assertIn("JSON 형식", completed.stderr)

    def test_invalid_transcript_output_returns_output_error(self) -> None:
        completed = self._run_cli(
            transcript_output_path=self.root / "transcript.json"
        )

        self.assertEqual(completed.returncode, ExitCode.OUTPUT_ERROR)
        self.assertIn("TXT 형식", completed.stderr)

    def test_existing_output_file_is_validated_without_modification(self) -> None:
        original = b"existing meeting output"
        self.output_path.write_bytes(original)

        completed = self._run_cli()

        self.assertEqual(completed.returncode, ExitCode.SUCCESS, completed.stderr)
        self.assertEqual(self.output_path.read_bytes(), original)

    def test_read_only_existing_output_returns_output_error(self) -> None:
        self.output_path.write_bytes(b"read only")
        self.output_path.chmod(stat.S_IREAD)
        try:
            completed = self._run_cli()
        finally:
            self.output_path.chmod(stat.S_IWRITE)

        self.assertEqual(completed.returncode, ExitCode.OUTPUT_ERROR)
        self.assertIn("출력 파일에 쓸 수 없습니다", completed.stderr)

    def test_missing_model_setting_returns_configuration_error(self) -> None:
        self.config_path.write_text(
            json.dumps({"stt_model": "local-whisper-model", "language": "ko"}),
            encoding="utf-8",
        )

        completed = self._run_cli()

        self.assertEqual(completed.returncode, ExitCode.CONFIGURATION_ERROR)
        self.assertIn("llm_model", completed.stderr)

    def test_external_api_setting_is_rejected(self) -> None:
        self._write_config(api_key="not-allowed")

        completed = self._run_cli()

        self.assertEqual(completed.returncode, ExitCode.CONFIGURATION_ERROR)
        self.assertIn("외부 API 설정은 사용할 수 없습니다", completed.stderr)

    def test_korean_transcript_is_written_through_nobodywho_adapter(self) -> None:
        with wave.open(str(self.input_path), "wb") as wav_file:
            wav_file.setnchannels(2)
            wav_file.setsampwidth(2)
            wav_file.setframerate(48_000)
            wav_file.writeframes(b"\x10\x00\x20\x00")

        fake_package = self.root / "fake-package"
        fake_package.mkdir()
        (fake_package / "nobodywho.py").write_text(
            """
import json

class _Stream:
    def __init__(self, result):
        self.result = result

    def completed(self):
        return self.result

class SpeechToText:
    def __init__(self, *, source, language):
        assert source == "local-whisper-model"
        assert language == "ko"

    def transcribe_pcm(self, samples, sample_rate):
        assert samples == [24]
        assert sample_rate == 48000
        return _Stream("오늘 CAN FD 통신 문제를 확인하겠습니다.")

class SamplerPresets:
    @staticmethod
    def constrain_with_json_schema(schema):
        assert schema["additionalProperties"] is False
        return "schema-constrained"

class Chat:
    def __init__(self, source, **kwargs):
        assert source == "local-llm-model"
        assert kwargs["sampler"] == "schema-constrained"
        assert kwargs["template_variables"] == {"enable_thinking": False}
        assert "Transcript에 없는 사실" in kwargs["system_prompt"]

    def ask(self, prompt):
        assert "오늘 CAN FD 통신 문제" in prompt
        return _Stream(json.dumps({
            "title": "CAN FD 통신 회의",
            "date": None,
            "summary": "CAN FD 통신 문제를 확인했다.",
            "topics": [{
                "topic": "CAN FD 통신",
                "discussion": "통신 문제를 확인했다."
            }],
            "decisions": [],
            "action_items": [],
            "open_issues": ["CAN FD 통신 문제"]
        }, ensure_ascii=False))
""".lstrip(),
            encoding="utf-8",
        )
        transcript_path = self.root / "meeting-transcript.txt"
        environment = os.environ.copy()
        environment["PYTHONPATH"] = os.pathsep.join(
            filter(None, (str(fake_package), environment.get("PYTHONPATH")))
        )

        completed = self._run_cli(
            transcript_output_path=transcript_path,
            environment=environment,
        )

        self.assertEqual(completed.returncode, ExitCode.SUCCESS, completed.stderr)
        response = json.loads(completed.stdout)
        self.assertEqual(response["status"], "completed")
        self.assertFalse(response["silence"])
        self.assertEqual(
            transcript_path.read_text(encoding="utf-8"),
            "오늘 CAN FD 통신 문제를 확인하겠습니다.\n",
        )
        meeting = json.loads(self.output_path.read_text(encoding="utf-8"))
        self.assertEqual(meeting["title"], "CAN FD 통신 회의")
        self.assertEqual(meeting["open_issues"], ["CAN FD 통신 문제"])

    def test_edited_transcript_is_analyzed_without_running_stt(self) -> None:
        transcript_path = self.root / "edited-transcript.txt"
        transcript_path.write_text(
            "MC33774 설정은 홍길동 님이 확인합니다. 기한은 정하지 않았습니다.",
            encoding="utf-8",
        )
        fake_package = self.root / "fake-llm-package"
        fake_package.mkdir()
        (fake_package / "nobodywho.py").write_text(
            """
import json

class _Stream:
    def completed(self):
        return json.dumps({
            "title": "제목 미정",
            "date": None,
            "summary": "MC33774 설정 확인을 요청했다.",
            "topics": [{
                "topic": "MC33774 설정",
                "discussion": "설정 확인 작업을 논의했다."
            }],
            "decisions": [],
            "action_items": [{
                "task": "MC33774 설정 확인",
                "owner": "홍길동",
                "due_date": None
            }],
            "open_issues": []
        }, ensure_ascii=False)

class SamplerPresets:
    @staticmethod
    def constrain_with_json_schema(schema):
        return schema

class Chat:
    def __init__(self, source, **kwargs):
        assert source == "local-llm-model"

    def ask(self, prompt):
        assert "MC33774" in prompt
        return _Stream()
""".lstrip(),
            encoding="utf-8",
        )
        environment = os.environ.copy()
        environment["PYTHONPATH"] = os.pathsep.join(
            filter(None, (str(fake_package), environment.get("PYTHONPATH")))
        )

        completed = self._run_cli(
            transcript_input_path=transcript_path,
            environment=environment,
        )

        self.assertEqual(completed.returncode, ExitCode.SUCCESS, completed.stderr)
        response = json.loads(completed.stdout)
        self.assertEqual(response["status"], "completed")
        self.assertEqual(Path(response["transcript_input"]), transcript_path)
        minutes = json.loads(self.output_path.read_text(encoding="utf-8"))
        self.assertEqual(minutes["action_items"][0]["owner"], "홍길동")
        self.assertIsNone(minutes["action_items"][0]["due_date"])

    def test_invalid_llm_json_returns_runtime_error_and_preserves_output(self) -> None:
        transcript_path = self.root / "edited-transcript.txt"
        transcript_path.write_text("회의 내용을 분석합니다.", encoding="utf-8")
        original = b"existing validated meeting"
        self.output_path.write_bytes(original)
        fake_package = self.root / "invalid-llm-package"
        fake_package.mkdir()
        (fake_package / "nobodywho.py").write_text(
            """
class _Stream:
    def completed(self):
        return "{not valid json"

class SamplerPresets:
    @staticmethod
    def constrain_with_json_schema(schema):
        return schema

class Chat:
    def __init__(self, source, **kwargs):
        pass

    def ask(self, prompt):
        return _Stream()
""".lstrip(),
            encoding="utf-8",
        )
        environment = os.environ.copy()
        environment["PYTHONPATH"] = os.pathsep.join(
            filter(None, (str(fake_package), environment.get("PYTHONPATH")))
        )

        completed = self._run_cli(
            transcript_input_path=transcript_path,
            environment=environment,
        )

        self.assertEqual(completed.returncode, ExitCode.RUNTIME_ERROR)
        self.assertIn("JSON 형식 또는 회의록 스키마", completed.stderr)
        self.assertNotIn("Traceback", completed.stderr)
        self.assertEqual(self.output_path.read_bytes(), original)

    def test_empty_edited_transcript_returns_input_error(self) -> None:
        transcript_path = self.root / "empty-transcript.txt"
        transcript_path.write_text(" \n", encoding="utf-8")

        completed = self._run_cli(transcript_input_path=transcript_path)

        self.assertEqual(completed.returncode, ExitCode.INPUT_ERROR)
        self.assertIn("빈 Transcript", completed.stderr)
        self.assertFalse(self.output_path.exists())

    def test_model_loading_error_returns_runtime_error_without_traceback(self) -> None:
        with wave.open(str(self.input_path), "wb") as wav_file:
            wav_file.setnchannels(1)
            wav_file.setsampwidth(2)
            wav_file.setframerate(16_000)
            wav_file.writeframes(b"\x01\x00")

        fake_package = self.root / "broken-package"
        fake_package.mkdir()
        (fake_package / "nobodywho.py").write_text(
            """
class SpeechToText:
    def __init__(self, *, source, language):
        raise RuntimeError("model files are missing")
""".lstrip(),
            encoding="utf-8",
        )
        environment = os.environ.copy()
        environment["PYTHONPATH"] = os.pathsep.join(
            filter(None, (str(fake_package), environment.get("PYTHONPATH")))
        )

        completed = self._run_cli(environment=environment)

        self.assertEqual(completed.returncode, ExitCode.RUNTIME_ERROR)
        self.assertIn("Whisper 모델을 불러오지 못했습니다", completed.stderr)
        self.assertNotIn("Traceback", completed.stderr)
        self.assertFalse((self.root / "transcript.txt").exists())

    def test_invalid_json_returns_configuration_error_without_traceback(self) -> None:
        self.config_path.write_text("{", encoding="utf-8")

        completed = self._run_cli()

        self.assertEqual(completed.returncode, ExitCode.CONFIGURATION_ERROR)
        self.assertIn("JSON 형식", completed.stderr)
        self.assertNotIn("Traceback", completed.stderr)

    def test_python_311_is_minimum_supported_runtime(self) -> None:
        self.assertFalse(python_version_is_supported((3, 10)))
        self.assertTrue(python_version_is_supported((3, 11)))
        self.assertTrue(python_version_is_supported((3, 14)))


if __name__ == "__main__":
    unittest.main()
