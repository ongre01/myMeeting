# AI 회의록 작성 프로그램 개발 사양서

**프로젝트명:** Local Meeting Minutes Assistant  
**대상 플랫폼:** Only Windows
**UI Framework:** Qt Widgets  
**AI Backend:** Python  
**Local AI Engine:** NobodyWho  
**Speech-to-Text:** Whisper via NobodyWho  
**LLM:** GGUF 기반 Local LLM via NobodyWho  
**Speaker Diarization:** 선택 기능, pyannote.audio 등 별도 모듈 검토  
**nobodywho github:** https://github.com/nobodywho-ooo/nobodywho


---

# 1. 개요

## 1.1 목적

본 프로젝트는 회의 음성을 녹음하고, 음성 내용을 자동으로 텍스트로 변환한 뒤 로컬 LLM을 이용하여 회의 요약, 주요 논의 사항, 결정 사항, Action Item 등을 자동 생성하는 데스크톱 회의록 작성 프로그램을 개발하는 것을 목적으로 한다.

회의 데이터는 가능한 한 로컬 PC에서 처리하여 외부 AI API 의존성을 최소화하고, 회사 내부 회의 내용이나 기술 정보가 외부 서버로 전송되지 않는 구조를 우선한다.

---

## 1.2 주요 목표

본 프로그램의 주요 목표는 다음과 같다.

- 마이크를 이용한 회의 음성 녹음
- 녹음 파일의 로컬 저장
- Whisper 기반 음성 인식
- 회의 전체 Transcript 생성
- Local LLM을 이용한 회의 내용 분석
- 주요 논의 사항 자동 추출
- 결정 사항 자동 추출
- Action Item 자동 추출
- 담당자 및 일정 정보 추출
- 회의록 Markdown/TXT/JSON 저장
- 사용자가 AI 생성 결과를 수정할 수 있는 UI 제공

---

# 2. 시스템 구성

전체 시스템은 Qt Widgets 기반 프론트엔드와 Python 기반 AI 백엔드로 분리한다.

```text
┌──────────────────────────────────────┐
│            Qt Widgets GUI            │
│                                      │
│  - 회의 녹음                         │
│  - 녹음 상태 표시                    │
│  - Transcript 표시                  │
│  - 회의록 표시/수정                  │
│  - 파일 저장                         │
└──────────────────┬───────────────────┘
                   │
                   │ Process / IPC
                   ▼
┌──────────────────────────────────────┐
│            Python AI Backend         │
│                                      │
│  - Speech-to-Text                    │
│  - LLM 요약                          │
│  - 구조화 데이터 생성                │
│  - Speaker Diarization (선택)        │
└──────────────────┬───────────────────┘
                   │
           ┌───────┴────────┐
           ▼                ▼
     NobodyWho          pyannote.audio
   Whisper / LLM        (Optional)
```

---

# 3. 개발 환경

## 3.1 Qt Application

- Qt 6.x
- Qt Widgets
- C++17 이상
- QMake 권장
- Visual Studio
- Windows 10/11

사용 모듈 예시:

```text
Qt::Widgets
Qt::Multimedia
Qt::Core
Qt::Network (향후 REST/WebSocket 사용 시)
```

---

## 3.2 Python AI Backend

권장 환경:

```text
Python 3.11+
```

주요 패키지:

```text
nobodywho
pyannote.audio     # 선택
torch              # diarization 사용 시
pydantic           # JSON 데이터 검증 권장
```

Python 가상환경 사용을 권장한다.

```bash
python -m venv .venv
```

---

# 4. 디렉터리 구조

권장 디렉터리 구조는 다음과 같다.

```text
meeting-minutes/
│
├─ app/
│  ├─ CMakeLists.txt
│  │
│  ├─ src/
│  │  ├─ main.cpp
│  │  ├─ MainWindow.cpp
│  │  ├─ MainWindow.h
│  │  │
│  │  ├─ audio/
│  │  │  ├─ AudioRecorder.cpp
│  │  │  └─ AudioRecorder.h
│  │  │
│  │  ├─ ai/
│  │  │  ├─ AiBackendClient.cpp
│  │  │  └─ AiBackendClient.h
│  │  │
│  │  ├─ meeting/
│  │  │  ├─ MeetingDocument.cpp
│  │  │  └─ MeetingDocument.h
│  │  │
│  │  └─ export/
│  │     ├─ MarkdownExporter.cpp
│  │     └─ MarkdownExporter.h
│  │
│  └─ ui/
│
├─ backend/
│  ├─ main.py
│  ├─ stt.py
│  ├─ summarizer.py
│  ├─ diarization.py
│  ├─ schema.py
│  └─ requirements.txt
│
├─ models/
│
├─ recordings/
│
├─ meetings/
│
├─ docs/
│  └─ DEVELOPMENT_SPEC.md
│
└─ README.md
```

---

# 5. 주요 기능

# 5.1 회의 녹음

Qt Multimedia의 `QAudioSource`를 사용하여 마이크 입력을 녹음한다.

필수 기능:

- 녹음 시작
- 일시정지
- 녹음 재개
- 녹음 종료
- 녹음 시간 표시
- 입력 장치 선택
- 녹음 파일 저장

권장 저장 형식:

```text
WAV
PCM 16-bit
16 kHz 또는 48 kHz
Mono
```

STT 입력을 고려하면 최종 Whisper 처리 단계에서 16 kHz mono PCM으로 변환 가능해야 한다.

---

# 5.2 녹음 파일 관리

녹음 파일은 기본적으로 다음 형태로 저장한다.

```text
recordings/
└─ 2026-09-29_103000_meeting.wav
```

파일명 기본 규칙:

```text
YYYY-MM-DD_HHMMSS_meeting.wav
```

사용자는 회의 제목을 입력하여 파일명을 변경할 수 있다.

---

# 5.3 Speech-to-Text

NobodyWho의 Speech-to-Text 기능을 이용해 Whisper 모델로 녹음 음성을 텍스트로 변환한다.

입력:

```text
meeting.wav
```

출력:

```text
Transcript
```

예:

```text
오늘 CAN FD 통신 문제부터 확인하겠습니다.
Arbitration bitrate는 정상적으로 설정되어 있습니다.
Data bitrate 설정을 다시 확인해보겠습니다.
```

한국어 회의의 경우 가능한 경우 언어를 `ko`로 지정한다.

---

# 5.4 Transcript 관리

Transcript는 AI 회의록 생성 이전에 사용자에게 표시한다.

사용자는 다음 작업을 할 수 있어야 한다.

- Transcript 확인
- Transcript 직접 수정
- 잘못 인식된 전문 용어 수정
- 특정 문장 삭제
- 회의록 AI 분석 다시 실행

Transcript는 별도 텍스트 파일로 저장 가능하도록 한다.

예:

```text
meetings/
└─ 2026-09-29/
   ├─ transcript.txt
   └─ meeting.json
```

---

# 5.5 AI 회의록 생성

Transcript를 Local LLM에 전달하여 회의 내용을 구조화한다.

기본 분석 항목:

- 회의 제목
- 회의 목적
- 전체 요약
- 주요 논의 사항
- 결정 사항
- Action Items
- 담당자
- 일정
- 미해결 이슈
- 후속 회의 필요 사항

---

# 5.6 회의록 데이터 구조

LLM의 자유로운 Markdown 출력 대신 JSON 기반 구조화 데이터를 기본으로 한다.

예:

```json
{
  "title": "BMS 개발 주간 회의",
  "date": "2026-09-29",
  "summary": "BMS 통신 문제 및 셀 밸런싱 진행 상황을 논의함.",
  "topics": [
    {
      "topic": "MC33774 통신",
      "discussion": "DADD enumeration 과정에서 응답 누락 문제를 확인함."
    }
  ],
  "decisions": [
    "CAN FD Data Bitrate 설정을 다시 검증한다."
  ],
  "action_items": [
    {
      "task": "MC33665 CAN FD 설정 확인",
      "owner": "유제환",
      "due_date": "2026-10-02"
    }
  ],
  "open_issues": [
    "MC33774 응답 누락 원인 분석 필요"
  ]
}
```

---

# 6. 데이터 Schema

Python Backend에서는 Pydantic 등을 이용하여 LLM 결과를 검증하는 것을 권장한다.

예시:

```python
class ActionItem:
    task: str
    owner: str | None
    due_date: str | None


class MeetingMinutes:
    title: str
    date: str | None
    summary: str
    topics: list
    decisions: list[str]
    action_items: list[ActionItem]
    open_issues: list[str]
```

구조화 결과가 Schema 검증에 실패할 경우 다음 중 하나를 수행한다.

1. LLM에 JSON 재생성 요청
2. 잘못된 필드만 보정
3. 사용자에게 오류 표시

---

# 7. AI Prompt 기본 사양

회의록 생성 Prompt는 다음 정보를 포함한다.

```text
당신은 기술 회의록 작성 도우미다.

아래 Transcript를 분석하여 회의록을 작성하라.

반드시 다음 정보를 추출하라.

- 회의 제목
- 회의 요약
- 주요 논의 사항
- 결정 사항
- Action Item
- 담당자
- 완료 목표일
- 미해결 이슈

Transcript에 존재하지 않는 내용은 임의로 생성하지 마라.
담당자 또는 일정이 불명확하면 null로 표시하라.

출력은 지정된 JSON Schema를 따라야 한다.
```

---

# 8. Speaker Diarization

## 8.1 개요

Whisper STT는 기본적으로 화자를 자동 식별하지 않는다.

예:

```text
오늘 CAN FD 문제부터 확인하겠습니다.
네, arbitration bitrate는 정상입니다.
그럼 data bitrate를 확인해 주세요.
```

화자 구분을 적용하면 다음 형태로 변환한다.

```text
[SPEAKER_00]
오늘 CAN FD 문제부터 확인하겠습니다.

[SPEAKER_01]
네, arbitration bitrate는 정상입니다.

[SPEAKER_00]
그럼 data bitrate를 확인해 주세요.
```

---

## 8.2 구현 방향

Speaker Diarization은 NobodyWho와 독립적인 모듈로 구현한다.

후보:

```text
pyannote.audio
```

초기 MVP에서는 제외 가능하다.

권장 개발 순서:

```text
Phase 1
Whisper STT만 적용

Phase 2
Speaker Diarization 추가

Phase 3
SPEAKER_00 → 실제 참석자 이름 매핑
```

---

# 9. Qt UI 사양

메인 화면은 다음 구조를 권장한다.

```text
┌─────────────────────────────────────────────────────────┐
│ AI Meeting Minutes                                     │
├─────────────────────────────────────────────────────────┤
│ 회의 제목: [____________________________________]       │
│                                                       │
│ 입력 장치: [Microphone ▼]                              │
│                                                       │
│ [● 녹음 시작] [Ⅱ 일시정지] [■ 종료]                   │
│                                                       │
│ 녹음 시간                                             │
│ 00:32:17                                              │
├─────────────────────────────────────────────────────────┤
│ Transcript                                            │
│                                                       │
│ 오늘 CAN FD 통신 문제부터 확인하겠습니다.             │
│ Arbitration bitrate는 정상입니다.                     │
│ Data bitrate 설정을 다시 확인하겠습니다.              │
│                                                       │
├─────────────────────────────────────────────────────────┤
│ [AI 회의록 생성]                                       │
├─────────────────────────────────────────────────────────┤
│ 회의 요약                                             │
│                                                       │
│ 주요 논의 사항                                         │
│                                                       │
│ 결정 사항                                             │
│                                                       │
│ Action Items                                          │
│                                                       │
├─────────────────────────────────────────────────────────┤
│ [Markdown 저장] [TXT 저장] [JSON 저장] [복사]          │
└─────────────────────────────────────────────────────────┘
```

---

# 10. Qt 주요 클래스

## 10.1 MainWindow

역할:

- 전체 UI 제어
- 회의 상태 관리
- 녹음 시작/종료 이벤트 처리
- Transcript 표시
- 회의록 표시
- Export 실행

---

## 10.2 AudioRecorder

역할:

- 입력 장치 관리
- QAudioSource 생성
- PCM 수집
- WAV 파일 생성
- 녹음 시간 관리

주요 인터페이스 예:

```cpp
class AudioRecorder : public QObject
{
    Q_OBJECT

public:
    bool startRecording(const QString& filePath);
    void pauseRecording();
    void resumeRecording();
    void stopRecording();

signals:
    void recordingStarted();
    void recordingStopped(QString filePath);
    void recordingError(QString message);
};
```

---

# 10.3 AiBackendClient

Qt와 Python Backend 간 통신을 담당한다.

MVP에서는 `QProcess` 사용을 권장한다.

예:

```text
meeting-ai.exe
    --input meeting.wav
    --output meeting.json
```

Qt 호출 예:

```text
QProcess
   │
   ├─ Python Backend 실행
   │
   └─ 완료 시 JSON 파일 읽기
```

향후 확장 시 REST 또는 WebSocket 구조로 변경 가능하다.

---

# 11. Python Backend 인터페이스

MVP CLI 예:

```bash
python main.py \
    --input "../recordings/meeting.wav" \
    --output "../meetings/meeting.json"
```

처리 과정:

```text
1. WAV 파일 검증
2. Whisper STT
3. Transcript 생성
4. LLM 분석
5. JSON Schema 검증
6. meeting.json 저장
```

---

# 12. 처리 상태

AI 작업 시간이 길 수 있으므로 UI에서는 작업 상태를 표시한다.

상태 예:

```text
Idle
Recording
Transcribing
Analyzing
Completed
Error
```

화면 표시 예:

```text
음성을 텍스트로 변환 중...

████████████░░░░░░

AI가 회의 내용을 분석하고 있습니다...
```

---

# 13. 파일 저장 구조

회의별 별도 디렉터리를 생성한다.

```text
meetings/
└─ 2026-09-29_103000/
   ├─ meeting.wav
   ├─ transcript.txt
   ├─ meeting.json
   └─ meeting.md
```

---

# 14. Markdown 회의록 형식

예:

```markdown
# BMS 개발 주간 회의

- 일시: 2026-09-29
- 참석자: 유제환, 홍길동

## 회의 요약

BMS CAN FD 통신 문제와 셀 밸런싱 개발 진행 상황을 논의하였다.

## 주요 논의 사항

### MC33774 통신

DADD Enumeration 과정에서 응답 누락 문제가 확인되었다.

## 결정 사항

- CAN FD Data Bitrate 설정을 재검증한다.

## Action Items

| 작업 | 담당자 | 기한 |
|---|---|---|
| MC33665 설정 확인 | 유제환 | 2026-10-02 |

## 미해결 이슈

- MC33774 응답 누락 원인 분석
```

---

# 15. Local LLM 모델

초기 개발에서는 작은 GGUF 모델을 권장한다.

후보 예:

```text
Qwen 계열
Gemma 계열
Mistral 계열
```

개발 초기에는 모델 다운로드 및 실행 여부 확인을 위해 1B 이하 소형 모델을 사용할 수 있다.

회의 요약 품질이 중요해지면 PC 사양에 따라 3B~8B 수준 모델을 검토한다.

---

# 16. 모델 관리

모델 경로는 설정 파일로 관리한다.

예:

```json
{
  "stt_model": "hf://onnx-community/whisper-base",
  "llm_model": "hf:NobodyWho/Qwen_Qwen3-0.6B-GGUF:Q4_K_M",
  "language": "ko"
}
```

설정 화면에서 모델 변경 기능을 향후 추가할 수 있다.

---

# 17. 개인정보 및 보안

기본 원칙:

- 회의 음성은 로컬 저장
- Transcript는 로컬 저장
- LLM inference는 로컬 실행
- 외부 AI API 호출 금지
- 모델 다운로드 외에는 인터넷 연결이 없어도 동작 가능하도록 설계

회사 내부 자료를 포함할 수 있으므로 사용자가 명시적으로 요청하지 않는 이상 클라우드 업로드 기능은 제공하지 않는다.

---

# 18. 오류 처리

주요 오류:

```text
마이크 장치 없음
녹음 실패
WAV 저장 실패
Python Backend 실행 실패
AI 모델 로딩 실패
메모리 부족
Whisper 분석 실패
LLM 응답 실패
JSON Schema 오류
```

사용자에게는 기술적인 Stack Trace 대신 이해 가능한 오류 메시지를 표시한다.

예:

```text
AI 모델을 실행하기 위한 메모리가 부족합니다.

더 작은 모델을 선택하거나 실행 중인 프로그램을 종료한 뒤 다시 시도하세요.
```

상세 로그는 별도 로그 파일에 저장한다.

---

# 19. 로그

로그 파일:

```text
logs/
└─ meeting-minutes.log
```

로그 대상:

- 프로그램 시작/종료
- 녹음 시작/종료
- Backend 실행
- STT 처리 시간
- LLM 처리 시간
- 모델 로드 오류
- 파일 저장 오류

회의 원문 및 음성 내용을 로그에 기록하지 않는 것을 기본 원칙으로 한다.

---

# 20. MVP 개발 범위

1차 버전에서는 다음 기능까지만 구현한다.

## 필수

- Qt Widgets UI
- 마이크 선택
- 녹음 시작/종료
- WAV 저장
- Python Backend 실행
- NobodyWho Whisper STT
- Transcript 표시
- NobodyWho Local LLM 회의록 생성
- JSON 기반 회의록 구조
- 회의록 UI 표시
- Markdown 저장
- TXT 저장
- JSON 저장

## 제외

초기 버전에서는 다음 기능을 제외한다.

- 실시간 STT
- Speaker Diarization
- 참석자 자동 식별
- 회의 중 실시간 요약
- RAG
- 회의 검색
- 클라우드 동기화
- Calendar 연동
- Teams/Zoom 연동

---

# 21. 개발 단계

## Phase 1 — Audio Recorder

목표:

```text
Microphone
   ↓
Qt
   ↓
meeting.wav
```

완료 조건:

- 마이크 선택 가능
- 녹음 가능
- WAV 정상 재생 가능

---

## Phase 2 — Speech-to-Text

목표:

```text
meeting.wav
    ↓
NobodyWho Whisper
    ↓
transcript.txt
```

완료 조건:

- 한국어 회의 음성 인식 가능
- Transcript 파일 생성

---

## Phase 3 — Meeting Analyzer

목표:

```text
Transcript
    ↓
NobodyWho Local LLM
    ↓
meeting.json
```

완료 조건:

- 회의 요약 생성
- 결정 사항 추출
- Action Item 추출
- JSON Schema 검증

---

## Phase 4 — Qt Integration

목표:

```text
Qt
 │
 ├─ 녹음
 ├─ STT 실행
 ├─ LLM 실행
 └─ 회의록 출력
```

완료 조건:

- GUI에서 전체 Workflow 실행 가능

---

## Phase 5 — Export

지원:

```text
Markdown
TXT
JSON
```

향후:

```text
DOCX
PDF
```

---

# 22. 향후 확장 기능

향후 다음 기능을 추가할 수 있다.

- 실시간 STT
- Speaker Diarization
- 참석자 이름 등록
- 참석자별 발언 검색
- 회의 중 실시간 요약
- 회의 종료 시 자동 회의록 생성
- 회의록 검색
- 과거 회의 기반 RAG
- 회의별 프로젝트 분류
- Action Item 추적
- 업무 관리 시스템 연동
- Google Calendar 연동
- Outlook 연동
- Jira 연동
- Slack 연동

---

# 23. 최종 목표

최종적으로 다음 Workflow를 제공한다.

```text
[회의 시작]
      │
      ▼
[녹음]
      │
      ▼
[회의 종료]
      │
      ▼
[Speech-to-Text]
      │
      ▼
[Transcript]
      │
      ▼
[Local LLM 분석]
      │
      ▼
[구조화 회의록]
      │
      ├─ 요약
      ├─ 논의 사항
      ├─ 결정 사항
      ├─ Action Item
      └─ 미해결 이슈
      │
      ▼
[사용자 수정]
      │
      ▼
[Markdown / TXT / JSON 저장]
```

본 프로젝트의 핵심 방향은 **회의 데이터를 외부 서버에 전달하지 않고 로컬 환경에서 회의 내용을 자동 정리할 수 있는 실용적인 데스크톱 회의록 도구**를 구현하는 것이다.
