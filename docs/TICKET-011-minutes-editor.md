# TICKET-011 회의록 편집 화면 구현

## 구현 범위

- AI 백엔드가 생성한 `meeting.json`을 로컬에서 읽어 C++ `MeetingMinutes` 메모리 모델로 변환한다.
- 회의 제목·일시·요약, 주요 논의, 결정 사항, Action Item, 미해결 이슈를 탭 형태의 Qt Widgets 편집 화면에 표시한다.
- 텍스트 변경과 표·목록 항목 편집을 즉시 현재 메모리 모델에 반영하고 `minutesChanged()` 신호를 발생시킨다.
- 주요 논의, 결정 사항, Action Item, 미해결 이슈는 항목 추가와 선택 삭제를 지원한다.
- 새 녹음을 시작하면 이전 회의록 모델과 화면을 함께 비워 서로 다른 회의의 결과가 섞이지 않게 한다.
- 녹음 또는 로컬 AI 백엔드 처리 중에는 회의록 편집 화면을 비활성화한다.

회의 음성, Transcript, 구조화 회의록은 기존과 동일하게 로컬 파일과 로컬 Python 프로세스만 사용한다. 외부 AI API 호출이나 업로드 경로는 추가하지 않았다.

## 메모리 모델 동기화

`meetingminutes.h/.cpp`의 모델은 Python `backend/schema.py`와 같은 구조를 사용한다.

```text
MeetingMinutes
├─ title / date / summary
├─ topics[]: topic / discussion
├─ decisions[]
├─ actionItems[]: task / owner / dueDate
└─ openIssues[]
```

`MainWindow`는 유효한 JSON을 읽은 뒤에만 모델을 교체하고 편집 화면을 채운다. 화면을 채우는 동안 발생하는 Qt 변경 신호는 차단 플래그로 무시하며, 이후 사용자 변경은 모든 편집 위젯에서 모델로 즉시 다시 수집한다. 빈 담당자와 기한, 빈 일시는 JSON Schema의 `null` 의미로 모델에 저장한다.

현재 모델은 `hasCurrentMinutes()`와 `currentMinutes()`로 확인할 수 있다. 화면 변경 시 `minutesChanged()`가 발생하므로 이후 저장·내보내기 티켓에서도 같은 메모리 모델을 사용할 수 있다.

## JSON 검증과 오류 처리

Qt 로더는 편집 화면에 데이터를 넣기 전에 다음을 검사한다.

- 최상위 값이 JSON 객체인지 확인
- `title`, `date`, `summary`, `topics`, `decisions`, `action_items`, `open_issues` 필드의 존재와 타입 확인
- 논의 항목과 Action Item 내부 필드의 타입 확인
- 문자열 목록에 문자열이 아닌 값이 포함됐는지 확인
- Python Pydantic 모델과 같이 알 수 없는 추가 필드 거부

구문 또는 Schema가 잘못된 `meeting.json`은 기존 모델을 덮어쓰지 않고 `Error — 처리 실패` 상태와 사용자용 오류 메시지를 표시한다.

## 빈 목록 표시

네 목록은 항목이 0개여도 정상적인 회의록으로 로드한다. 각 탭에는 “항목이 없습니다” 안내를 표시하며, 사용자는 바로 “추가” 버튼으로 첫 항목을 만들 수 있다. 첫 항목이 추가되면 안내가 사라지고 모델의 해당 목록도 즉시 갱신된다.

## 자동 검증

2026-09-30 Windows 11, Qt 6.11.0 MSVC 2022 64-bit 환경에서 확인했다.

- Qt 앱 Debug/Release 빌드 성공
- Qt Test Debug/Release 각각 27건 통과, 실패 및 건너뜀 없음
- Debug/Release 앱이 offscreen 환경에서 즉시 종료하지 않고 이벤트 루프에 진입함
- 기존 녹음, 저장소, WAV, 백엔드, Transcript 편집 회귀 테스트 통과
- 구조화 결과 표시와 모든 필드의 메모리 모델 동기화 테스트 통과
- 네 빈 목록의 안내, 첫 항목 추가 및 모델 반영 테스트 통과
- 잘못된 JSON의 오류 상태와 편집 비활성화 테스트 통과

offscreen 테스트 환경에서는 Qt 설치에 글꼴 디렉터리가 없다는 경고가 출력되지만 테스트 결과와 앱 이벤트 루프 진입에는 영향을 주지 않았다. 실제 NobodyWho 모델의 회의록 품질과 실제 화면의 시각적 배치는 대상 PC에서 별도로 수동 확인해야 한다.
