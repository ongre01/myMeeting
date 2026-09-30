# TICKET-005 백엔드 CLI와 모델 설정 구현

## 구현 범위

- Python 3.11 이상에서 실행하는 로컬 백엔드 진입점 `backend/main.py`를 추가했다.
- `--input`, `--output`, `--config` 인수를 검증한다. `--config`를 생략하면 현재 작업 폴더와 무관하게 `backend/config.json`을 사용한다.
- 이 티켓에서는 STT나 LLM을 실행하거나 출력 파일을 만들지 않는다. 검증 성공 시 후속 티켓이 처리 단계를 연결할 수 있도록 한 줄짜리 JSON 결과를 표준 출력에 기록한다.
- Python 외부 패키지는 아직 필요하지 않으며 `backend/requirements.txt`에 이를 명시했다.

## 실행 방법

저장소 루트에서 Python 가상 환경을 만들고 CLI를 실행한다.

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -r backend\requirements.txt
python backend\main.py `
    --input recordings\meeting.wav `
    --output meetings\meeting.json
```

다른 설정 파일을 사용할 때는 `--config path\to\config.json`을 추가한다. 성공 시 다음 구조를 한 줄로 출력하고 종료 코드 0을 반환한다.

```json
{"status":"validated","input":"<절대 WAV 경로>","output":"<절대 JSON 경로>","config":"<절대 설정 경로>","language":"ko"}
```

## 모델 설정

설정 파일은 UTF-8 JSON 객체이며 다음 세 문자열만 허용한다.

```json
{
  "stt_model": "hf://onnx-community/whisper-base",
  "llm_model": "hf:NobodyWho/Qwen_Qwen3-0.6B-GGUF:Q4_K_M",
  "language": "ko"
}
```

모델 값은 NobodyWho 연동 티켓에서 사용할 로컬 추론 모델 식별자다. 이 단계에서는 모델을 다운로드하거나 로드하지 않으므로 식별자가 비어 있지 않은지만 검사한다. `api_key`, `endpoint`, `base_url`을 포함한 추가 키는 모두 거부한다. 프로세스 환경의 API 키도 읽지 않으며 네트워크 요청 코드나 외부 AI API 호출 코드가 없다.

## 경로 검증

- 입력은 존재하고 읽을 수 있는 일반 파일이어야 하며 확장자는 대소문자와 무관하게 `.wav`여야 한다. WAV 내용 검증은 TICKET-006의 범위다.
- 출력 확장자는 `.json`이어야 한다.
- 출력 상위 폴더는 이미 존재하는 폴더여야 하며, 임시 파일을 생성·삭제하는 방식으로 쓰기 가능 여부를 확인한다.
- 기존 출력 경로가 있으면 일반 파일이고 쓰기 가능한지 확인한다. 이 티켓에서는 해당 파일을 변경하지 않는다.

## 종료 코드와 오류 출력

| 종료 코드 | 의미 |
|---:|---|
| 0 | 모든 검증 성공 |
| 2 | 필수 인수 누락 등 명령줄 사용법 오류 |
| 3 | 입력 WAV 경로 오류 |
| 4 | 출력 JSON 경로 오류 |
| 5 | 설정 파일 또는 모델 설정 오류 |
| 6 | Python 버전 등 실행 환경 오류 |

표준 출력과 표준 오류는 Windows 시스템 코드페이지와 무관하게 UTF-8을 사용한다. 검증 오류는 `오류:`로 시작하는 사용자용 한국어 메시지를 표준 오류에 기록하며 Python 스택 트레이스는 출력하지 않는다.

## 자동 검증

표준 라이브러리 `unittest`로 다음을 실행한다.

```powershell
python -m unittest discover -s backend\tests -v
```

테스트는 유효한 경로와 기본 설정, Python 3.11 최소 버전, 없는 WAV, 잘못된 확장자, 없는 출력 폴더, 기존·읽기 전용 출력 파일, 누락된 모델 설정, 잘못된 JSON, 외부 API 설정 거부를 포함한다. 성공 검증 뒤 새 출력 JSON 파일을 만들거나 기존 파일 내용을 변경하지 않는 것도 확인한다.

## 구현 검증 결과

Windows 11 환경에서 다음을 확인했다.

- Python 3.14.7에서 `compileall` 성공
- Python 3.12, 3.13, 3.14에서 백엔드 `unittest` 각각 12건 통과, 실패 없음
- Qt 6.11.0 MSVC 2022 64-bit 앱 Debug/Release 빌드 성공
- 기존 Qt Test Debug/Release 각각 16건 통과, 실패 및 건너뜀 없음
- Debug/Release 앱이 즉시 종료하지 않고 이벤트 루프에 진입함
