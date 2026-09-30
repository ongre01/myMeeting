# TICKET-010 Transcript 표시와 편집 구현

## 구현 범위

- STT가 만든 `transcript.txt`를 Qt `QPlainTextEdit`에 표시하고 사용자가 직접 수정할 수 있다.
- “Transcript 저장”은 UTF-8 텍스트를 `QSaveFile`로 원자적으로 저장한다. 쓰기 또는 commit 실패 시 기존 파일을 보존하고 사용자 메시지를 표시한다.
- “다시 불러오기”는 현재 회의의 `transcript.txt`를 다시 읽어 저장된 수정 내용을 편집기에 복원한다.
- Transcript가 준비되면 기존 생성 버튼은 “수정본으로 다시 분석”으로 바뀐다. 클릭 시 편집 내용을 먼저 저장하고, 같은 회의 폴더의 `meeting.json`을 갱신한다.
- Transcript 편집 및 분석 중에도 음성과 텍스트는 로컬 파일과 로컬 Python 프로세스로만 전달한다. 외부 API나 업로드 경로는 추가하지 않았다.

## 처리 흐름

최초 AI 처리는 TICKET-009 흐름을 유지한다.

```text
meeting.wav
  → backend/main.py --input ... --transcript-output transcript.txt
  → transcript.txt 로드
  → Qt 편집기에 표시
```

사용자가 내용을 수정한 뒤 다시 분석하면 WAV를 다시 STT하지 않는다.

```text
Qt 편집기의 수정본
  → QSaveFile로 transcript.txt 저장
  → backend/main.py --transcript-input transcript.txt --output meeting.json
  → 로컬 LLM 분석
  → 수정본을 다시 편집기에 로드
```

`AiBackendClient::analyzeTranscript()`는 `--transcript-input` 전용 비동기 실행 경로를 제공한다. 실행 전 파일이 읽을 수 있는지와 공백이 아닌 내용이 있는지 검사하고, 실행 중에는 기존과 동일하게 중복 요청을 거부한다. 완료 시에도 Transcript 파일과 JSON 회의록을 검증한 뒤에만 성공 신호를 보낸다.

## UI와 상태 제어

- 최초 STT 완료 전에는 Transcript 편집기, 저장, 다시 불러오기 버튼을 비활성화한다.
- 백엔드 작업 또는 녹음 중에는 Transcript 편집과 파일 동작을 비활성화한다.
- 편집 내용이 공백뿐이면 재분석 버튼을 즉시 비활성화한다.
- 비활성화된 버튼을 우회해 슬롯이 호출되어도 빈 Transcript 분석을 다시 검사하고 사용자 오류로 처리한다.
- 새 녹음을 시작하면 이전 회의의 Transcript 경로와 편집 내용을 지워 회의 간 데이터가 섞이지 않게 한다.

## 오류와 경계 조건

- Transcript 경로가 없거나 파일을 읽을 수 없으면 저장/재불러오기/분석을 시작하지 않는다.
- 빈 파일과 공백뿐인 Transcript는 로컬 LLM 실행 전에 차단한다.
- 저장할 때 UTF-8을 사용하고 비어 있지 않은 파일에는 마지막 줄바꿈 하나를 보장한다.
- 다시 불러올 때 저장 규칙으로 추가된 마지막 줄바꿈 하나를 편집 화면에서 제거한다.
- 재분석 실패 시 편집 내용과 저장된 Transcript는 남아 있어 사용자가 수정 후 재시도할 수 있다.

## 자동 검증

Qt 테스트는 임시 회의 폴더와 로컬 fake Python 백엔드를 사용하며 외부 API나 모델 다운로드를 사용하지 않는다.

- 전문 용어 수정 후 UTF-8 `transcript.txt` 저장과 다시 불러오기
- 편집 내용을 `--transcript-input`으로 전달하고 그 수정본으로 `meeting.json` 재생성
- 공백뿐인 Transcript의 버튼 비활성화와 슬롯 수준 이중 차단
- 기존 녹음, 저장소, WAV, 백엔드 비동기 처리 및 오류 처리 회귀 테스트

2026-09-30 Windows 11, Qt 6.11.0 MSVC 2022 64-bit 환경에서 다음을 확인했다.

- Qt 앱 Debug/Release 빌드 성공, 컴파일 warning 없음
- Qt Test Debug/Release 각각 24건 통과, 실패 및 건너뜀 없음
- Python `unittest` 47건 통과
- Python `compileall` 성공
- Debug/Release 앱이 offscreen 환경에서 즉시 종료하지 않고 이벤트 루프에 진입함

실제 NobodyWho Whisper/GGUF 모델의 인식·요약 품질은 모델 파일과 대상 하드웨어가 필요한 수동 검증 항목이다.
