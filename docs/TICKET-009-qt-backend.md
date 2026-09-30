# TICKET-009 Qt와 Python 작업 연동 구현

## 구현 범위

- `AiBackendClient`가 `QProcess`로 `backend/main.py`를 비동기 실행한다. GUI 스레드에서 `waitForStarted()`나 `waitForFinished()`를 호출하지 않는다.
- 녹음이 완료되면 “AI 회의록 생성” 버튼을 활성화하고, 사용자가 누르면 같은 회의 폴더의 `meeting.wav`를 입력으로 사용해 `transcript.txt`와 `meeting.json`을 생성한다.
- 처리 중에는 녹음 시작, 장치 변경, 새로 고침, 중복 AI 요청을 비활성화한다. 슬롯과 `AiBackendClient::start()`도 실행 상태를 다시 검사해 중복 요청을 차단한다.
- UI에 `Idle`, `Transcribing`, `Analyzing`, `Completed`, `Error` 상태와 한국어 사용자 메시지, 무한 진행 표시를 연결했다.
- 회의 음성, Transcript, 회의록은 명령줄 인수와 로컬 파일로만 전달한다. 외부 AI API 호출이나 업로드 경로는 추가하지 않았다.

## 백엔드 진행 프로토콜

기존 CLI 사용자의 한 줄 최종 JSON 출력은 유지한다. Qt 클라이언트는 `--progress`를 추가하고 다음 JSON Lines 이벤트를 표준 출력에서 비동기로 읽는다.

```json
{"status": "transcribing"}
{"status": "analyzing"}
{"status": "completed", "output": "...", "transcript_output": "..."}
```

무음 WAV는 STT가 빈 `transcript.txt`를 저장한 뒤 최종 상태 `transcribed`를 반환한다. 이 경우 Qt는 오류가 아닌 `Completed`로 표시하고 회의록 JSON이 생성되지 않았음을 사용자에게 알린다.

표준 오류는 종료 코드가 0이 아닐 때만 사용자 오류로 처리한다. 백엔드가 약속한 `오류:` 접두사의 UTF-8 메시지만 그대로 전달하며, 그 밖의 프로세스 출력은 종료 코드가 포함된 일반 메시지로 바꿔 Transcript나 예기치 않은 내부 출력이 UI에 노출되지 않게 한다.

## 실행 파일과 스크립트 검색

소스 트리에서 처음 실행하기 전에 프로젝트 전용 가상환경을 준비한다.

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r backend\requirements.txt
```

설치 후 이미 실행 중인 앱은 종료하고 다시 시작해야 한다.

기본 실행에서는 다음 순서로 Python과 백엔드 스크립트를 찾는다.

1. `MYMEETING_PYTHON_EXECUTABLE`, `MYMEETING_BACKEND_SCRIPT` 환경 변수
2. 실행 파일 또는 현재 작업 폴더의 상위 디렉터리에 있는 `.venv/Scripts/python.exe`, `backend/main.py`
3. `PATH`의 `python` 또는 `python3`

소스 트리 밖에 배포할 때는 두 환경 변수에 절대 경로를 지정하거나 배포 디렉터리에 `.venv`와 `backend`를 함께 둔다. 모델 설정은 기존 규칙대로 백엔드 스크립트 옆의 `config.json`을 사용한다.

## 성공 결과 검증

프로세스 종료 코드 0만으로 성공 처리하지 않는다.

- 최종 JSON 상태가 `completed` 또는 `transcribed`인지 확인한다.
- `transcript.txt`가 읽을 수 있는 일반 파일인지 확인한다.
- `completed`일 때 `meeting.json`이 읽을 수 있고 올바른 JSON 객체인지 확인한다.
- 표준 출력의 JSON Lines가 손상됐거나, 최종 결과 또는 출력 파일이 없으면 `Error`로 전환한다.

검증이 끝난 뒤에만 `completed(outputPath, transcriptPath)` 신호를 내보낸다. 이후 Transcript/회의록 편집 티켓은 이 신호와 검증된 로컬 경로를 사용할 수 있다.

## 오류와 경계 조건

- Python 실행 파일이나 `backend/main.py`가 없으면 프로세스를 시작하지 않고 경로 오류를 표시한다.
- 비정상 종료와 0이 아닌 종료 코드를 구분해 `Error`로 처리한다.
- 실행 중 두 번째 `start()`는 `false`를 반환하며 기존 작업은 계속 실행한다.
- 종료 코드 0이어도 출력 파일이 없거나 JSON이 잘못되면 성공 신호를 보내지 않는다.
- 처리 중 창이 닫히면 백엔드 프로세스를 먼저 종료하고, 짧은 유예 후에도 남아 있으면 강제 종료한다.

## 자동 검증 결과

2026-09-30 Windows 11, Qt 6.11.0 MSVC 2022 64-bit 환경에서 확인했다.

- Python `unittest` 47건 통과
- Python `compileall` 성공
- Qt 앱 Debug/Release 빌드 성공
- Qt Test Debug/Release 각각 21건 통과, 실패 및 건너뜀 없음
- Debug/Release 앱이 offscreen 환경에서 즉시 종료하지 않고 이벤트 루프에 진입함

Qt 테스트는 임시 로컬 Python 백엔드를 실행해 다음을 검증한다.

- `Transcribing → Analyzing → Completed` 상태와 UI 결과 전달
- 실행 중 중복 요청 차단
- 실행 파일 부재와 비정상 종료 오류
- 종료 코드 0이지만 `meeting.json`이 없는 결과 거부
- `transcript.txt`와 유효한 JSON 결과 파일 확인 후에만 완료 처리

자동 테스트는 외부 API와 모델 다운로드를 사용하지 않는다. 실제 NobodyWho Whisper/GGUF 모델의 처리 시간과 품질은 로컬 모델 파일 및 대상 하드웨어를 준비한 뒤 별도로 확인해야 한다.
