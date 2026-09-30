# TICKET-007 구조화 회의록 스키마 구현

## 구현 범위

- `backend/schema.py`에 Pydantic v2 기반 `MeetingMinutes`, `Topic`, `ActionItem` 자료형을 추가했다.
- 회의록 최상위 필드 `title`, `date`, `summary`, `topics`, `decisions`, `action_items`, `open_issues`를 모두 필수로 검증한다.
- 각 주요 논의 사항은 `topic`과 `discussion`, 각 Action Item은 `task`, `owner`, `due_date`를 반드시 포함한다.
- 회의 날짜, 담당자, 기한을 알 수 없을 때는 각각 `date`, `owner`, `due_date`에 JSON `null`을 사용할 수 있다. 필드 자체를 생략하는 것은 허용하지 않는다.
- 이 스키마는 로컬 데이터만 검증하며 네트워크 또는 외부 AI API를 호출하지 않는다.

## 검증 정책

`MeetingMinutes.model_validate_json(json_text)`로 LLM의 JSON 문자열을 직접 검증할 수 있다. Python 객체를 검증할 때는 `MeetingMinutes.model_validate(data)`를 사용한다.

검증은 다음 원칙을 따른다.

- 선언한 자료형과 다른 값은 자동 변환하지 않고 거부한다.
- 스키마에 없는 필드는 거부한다.
- `topics`, `decisions`, `action_items`, `open_issues`는 빈 배열일 수 있다.
- `date`, `owner`, `due_date`는 문자열 또는 `null`이어야 한다.
- 그 외 문자열 필드는 문자열이어야 한다.

예시:

```python
from backend.schema import MeetingMinutes

minutes = MeetingMinutes.model_validate_json(llm_json)
validated_json = minutes.model_dump_json(indent=2)
```

Pydantic 검증에 실패하면 `pydantic.ValidationError`가 발생한다. 후속 로컬 LLM 티켓은 이 오류를 사용해 잘못된 JSON에 대한 보정 또는 사용자 오류 처리를 수행할 수 있다.

## 의존성

재현 가능한 설치를 위해 `backend/requirements.txt`에 `pydantic==2.12.5`를 고정했다.

```powershell
python -m pip install -r backend\requirements.txt
```

## 자동 검증

저장소 루트에서 다음 명령을 실행한다.

```powershell
python -m unittest discover -s backend\tests -v
python -m compileall -q backend
```

`backend/tests/test_schema.py`는 프로젝트 예시 JSON, 최상위 및 중첩 필수 필드 누락, nullable 날짜·담당자·기한, 잘못된 타입, 미정의 필드 거부를 확인한다.

## 검증 결과

2026-09-30 Windows 환경에서 다음을 확인했다.

- Python 3.14.7에서 백엔드 `unittest` 33건 통과
- Python `compileall` 및 `pip check` 성공
- Qt 6.11.0 MSVC 2022 64-bit 앱 Debug/Release 빌드 성공
- 기존 Qt Test Debug/Release 종료 코드 0
- Debug/Release 앱이 즉시 종료하지 않고 이벤트 루프에 진입함
