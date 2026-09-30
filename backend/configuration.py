"""Loading and validation for the local backend model configuration."""

from __future__ import annotations

from dataclasses import dataclass
import json
from pathlib import Path
import re
from typing import Any


_REQUIRED_KEYS = frozenset({"stt_model", "llm_model", "language"})
_LANGUAGE_PATTERN = re.compile(r"^[A-Za-z]{2,8}(?:-[A-Za-z0-9]{1,8})*$")


class ConfigurationError(ValueError):
    """Raised when the backend configuration cannot be used safely."""


@dataclass(frozen=True, slots=True)
class ModelConfiguration:
    """Validated identifiers for local NobodyWho inference."""

    stt_model: str
    llm_model: str
    language: str


def _object_without_duplicate_keys(
    pairs: list[tuple[str, Any]],
) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ConfigurationError(f"설정 키가 중복되었습니다: {key}")
        result[key] = value
    return result


def _required_text(settings: dict[str, Any], key: str) -> str:
    value = settings[key]
    if not isinstance(value, str) or not value.strip():
        raise ConfigurationError(f"필수 모델 설정이 비어 있습니다: {key}")
    return value.strip()


def load_model_configuration(config_path: Path) -> ModelConfiguration:
    """Read a UTF-8 JSON file and return its validated local model settings."""

    try:
        source = config_path.read_text(encoding="utf-8")
    except FileNotFoundError as error:
        raise ConfigurationError(
            f"모델 설정 파일을 찾을 수 없습니다: {config_path}"
        ) from error
    except (OSError, UnicodeError) as error:
        raise ConfigurationError(
            f"모델 설정 파일을 읽을 수 없습니다: {config_path} ({error})"
        ) from error

    try:
        settings = json.loads(source, object_pairs_hook=_object_without_duplicate_keys)
    except ConfigurationError:
        raise
    except json.JSONDecodeError as error:
        raise ConfigurationError(
            "모델 설정 JSON 형식이 잘못되었습니다: "
            f"{config_path} ({error.lineno}행 {error.colno}열)"
        ) from error

    if not isinstance(settings, dict):
        raise ConfigurationError("모델 설정의 최상위 값은 JSON 객체여야 합니다.")

    keys = set(settings)
    missing_keys = sorted(_REQUIRED_KEYS - keys)
    if missing_keys:
        raise ConfigurationError(
            f"필수 모델 설정이 누락되었습니다: {', '.join(missing_keys)}"
        )

    unknown_keys = sorted(keys - _REQUIRED_KEYS)
    if unknown_keys:
        raise ConfigurationError(
            "허용되지 않은 설정입니다: "
            f"{', '.join(unknown_keys)}. 외부 API 설정은 사용할 수 없습니다."
        )

    stt_model = _required_text(settings, "stt_model")
    llm_model = _required_text(settings, "llm_model")
    language = _required_text(settings, "language")
    if _LANGUAGE_PATTERN.fullmatch(language) is None:
        raise ConfigurationError(
            "language는 'ko' 또는 'ko-KR'과 같은 언어 태그여야 합니다."
        )

    return ModelConfiguration(
        stt_model=stt_model,
        llm_model=llm_model,
        language=language,
    )

