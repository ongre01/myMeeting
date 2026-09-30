# TICKET-013 로컬 로그와 통합 검증

## 구현 범위

- 애플리케이션 실행 파일 옆의 `logs/meeting-minutes.log`에 UTF-8 JSON Lines 형식의 로컬 로그를 추가했다.
- 앱 시작/종료, 녹음 시작/완료/오류, 백엔드 시작/완료/실패, STT와 LLM 분석 단계의 시작/완료/실패 및 처리 시간, Markdown/TXT/JSON 내보내기 결과와 처리 시간을 기록한다.
- 로그 폴더가 없으면 시작 시 생성한다. 로그 파일을 만들 수 없더라도 녹음과 AI 처리 자체는 계속 사용할 수 있다.
- 기존 로컬 `QProcess` → Python → NobodyWho 흐름만 사용하며 외부 AI API 전송 경로를 추가하지 않았다. 백엔드 설정은 `stt_model`, `llm_model`, `language` 이외의 키를 거부하므로 API URL이나 인증 키를 설정할 수 없다.

## 내용 비기록 설계

로거의 공개 함수는 자유 문자열을 받지 않는다. 미리 정의한 이벤트 enum, 밀리초 처리 시간, 내보내기 형식 enum만 받을 수 있다. 따라서 다음 값은 로그에 기록되지 않는다.

- WAV 바이트 또는 음성 내용
- Transcript 원문과 문자 수
- LLM 프롬프트, 응답, 회의록 필드
- 회의 제목, 파일명, 전체 경로
- 백엔드 표준 출력/표준 오류 및 사용자용 오류문
- 모델 이름과 설정 내용

각 로그 행의 허용 필드는 `timestamp`, `level`, `event`, 선택적인 `duration_ms`, 선택적인 `format`뿐이다. Qt의 일반 메시지 핸들러를 가로채지 않으므로 다른 라이브러리가 출력한 문자열도 이 파일로 유입되지 않는다.

예시:

```json
{"event":"transcription_completed","duration_ms":1523,"level":"info","timestamp":"2026-09-30T01:23:45.678Z"}
{"event":"export_completed","duration_ms":2,"format":"markdown","level":"info","timestamp":"2026-09-30T01:24:01.234Z"}
```

## 자동 통합 검증

Qt 통합 테스트 `completesWorkflowAfterModelFailureWithoutLoggingContent`는 다음 흐름을 한 번에 수행한다.

1. 가짜 마이크로 로컬 WAV를 생성한다.
2. 첫 번째 백엔드 실행에서 STT 후 모델 분석 실패를 발생시킨다.
3. 같은 화면에서 다시 실행하여 Transcript와 구조화 회의록을 생성한다.
4. Markdown, TXT, JSON을 모두 저장하고 네 산출물(`transcript.txt` 포함)이 존재하는지 확인한다.
5. 로그에서 실패와 재시작, 최종 완료 및 세 내보내기 형식을 확인한다.
6. 회의 제목, Transcript, LLM 결과, 모델 오류문, 임시 전체 경로에 넣은 고유 표식이 로그에 없고 모든 로그 키가 허용 목록 안에 있는지 확인한다.

## 수동 종단 간 절차

### 사전 조건

1. Windows 10/11, Qt 6 MSVC 2022 x64 빌드와 Python 3.11 이상 환경을 준비한다.
2. `backend/config.json`에 로컬 NobodyWho STT/LLM 모델과 `language`만 설정한다.
3. 네트워크가 필요한 최초 모델 다운로드를 미리 끝낸 뒤 네트워크를 끊어도 추론 가능한 상태인지 확인한다.
4. 기존 `logs/meeting-minutes.log`를 보관하거나 삭제해 이번 실행 구간을 쉽게 구분한다.

### 정상 워크플로

1. 앱을 시작하고 마이크와 회의 제목을 선택한다.
2. `녹음 시작` 후 식별하기 쉬운 테스트 문장을 말하고 `종료`를 누른다.
3. `AI 회의록 생성`을 눌러 `Transcribing`, `Analyzing`, `Completed` 순서로 진행되는지 확인한다.
4. Transcript와 회의록을 수정하고 Transcript 저장 및 수정본 재분석을 확인한다.
5. `Markdown 저장`, `TXT 저장`, `JSON 저장`을 각각 누른다.
6. 회의 폴더에 `meeting.wav`, `transcript.txt`, `meeting.json`, `meeting.md`, `meeting.txt`가 있고 내용을 로컬에서 열 수 있는지 확인한다.
7. 앱을 정상 종료한다.
8. 로그에서 `application_started`/`application_stopped`, 녹음 이벤트, STT/분석의 `duration_ms`, `backend_completed`, 세 형식의 `export_completed`를 확인한다.

### 모델 실패와 복구

1. 테스트용으로 `backend/config.json`의 `llm_model`을 존재하지 않는 로컬 모델 식별자로 임시 변경한다.
2. 녹음 후 AI 회의록 생성을 실행하고 UI가 `Error — 처리 실패`를 표시하는지 확인한다.
3. 로그에 `analysis_failed`와 `backend_failed`가 처리 시간과 함께 남고, UI 오류문이나 잘못된 모델 식별자는 기록되지 않았는지 확인한다.
4. 올바른 로컬 모델 설정을 복원하고 같은 화면에서 AI 회의록 생성을 다시 실행한다.
5. 작업이 완료되고 내보내기 버튼을 사용할 수 있는지 확인한다. 로그에는 새 `backend_started` 뒤 `backend_completed`가 있어야 한다.

### 내용 비기록 경계 조건

1. 회의 제목, 발화, Transcript 수정본, 회의록 요약에 각각 서로 다른 고유 문자열을 넣는다.
2. 전체 워크플로와 세 형식 저장을 끝낸 뒤 아래 PowerShell 검사에서 결과가 없는지 확인한다.

```powershell
Select-String -LiteralPath .\logs\meeting-minutes.log -Pattern '<회의제목표식>','<발화표식>','<Transcript표식>','<회의록표식>'
```

3. 각 행을 JSON으로 파싱했을 때 허용 필드 외 키가 없는지 확인한다.
4. 실패 시에도 `message`, `path`, `content`, `transcript`, `prompt`, `response` 같은 필드가 추가되지 않는지 확인한다.

## 검증 명령

Developer PowerShell에서 프로젝트 루트를 기준으로 실행한다.

```powershell
qmake myMeeting.pro -o build-app-debug/Makefile
nmake /NOLOGO /F build-app-debug/Makefile debug

qmake tests/tests.pro -o build-tests-debug/Makefile
nmake /NOLOGO /F build-tests-debug/Makefile debug
$env:QT_QPA_PLATFORM = 'offscreen'
.\build-tests-debug\debug\tst_appshell.exe
Remove-Item Env:QT_QPA_PLATFORM
```

Release도 같은 방식으로 `CONFIG+=release`와 Release 빌드 폴더를 사용한다. 실제 마이크, 실제 로컬 모델 처리, 화면 배치와 오디오 재생은 자동 테스트가 대신할 수 없으므로 위 수동 절차로 별도 확인한다.

## 구현 시 검증 결과

2026-09-30 Windows 11, Qt 6.11.0 MSVC 2022 64-bit, Python 3.11 이상 가상환경에서 확인했다.

- Qt 앱 Debug/Release 빌드 성공
- Qt Test Debug/Release 각각 31건 통과, 실패 및 건너뜀 없음
- Python 백엔드 단위/CLI 테스트 47건 통과
- Debug/Release 앱이 offscreen 환경에서 즉시 종료하지 않고 이벤트 루프에 진입함
- 자동 통합 테스트에서 모델 실패 후 재시도, WAV→Transcript→회의록→Markdown/TXT/JSON 저장 완료
- 자동 통합 테스트에서 제목·Transcript·LLM 결과·오류문·전체 경로 표식 비기록 및 로그 키 허용 목록 준수 확인

offscreen 환경에는 네이티브 창 닫기 경로가 없어 앱 실행 확인 프로세스는 이벤트 루프 진입 후 테스트 도구로 종료했다. 따라서 실제 창의 정상 종료와 실제 마이크/로컬 모델 워크플로는 위 수동 절차의 확인 대상으로 남긴다. Qt 설치의 글꼴 디렉터리 부재 경고가 있었지만 테스트 결과에는 영향을 주지 않았다.
