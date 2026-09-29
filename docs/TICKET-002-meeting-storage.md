# TICKET-002 회의별 로컬 저장소 구현

## 구현 범위

- `MeetingStorage`가 지정된 로컬 `meetings` 루트 아래에 `yyyy-MM-dd_HHmmss` 형식의 회의 폴더를 생성한다.
- 제목이 있으면 폴더명 뒤에 제목을 붙인다. Windows에서 사용할 수 없는 문자, 경로 구분자, 제어 문자는 `_`로 바꾸고 제목 길이는 80자로 제한한다.
- 같은 시각과 제목으로 연속 생성하면 `_001`, `_002` 순서의 접미사를 붙인다. 디렉터리 생성 자체를 충돌 확인에 사용하므로 이미 존재하는 회의 폴더를 덮어쓰지 않는다.
- 생성 결과로 다음 절대 경로를 제공한다.
  - `meeting.wav`
  - `transcript.txt`
  - `meeting.json`
  - `meeting.md`
- 저장 루트와 회의 폴더 생성 실패, 회의 폴더 쓰기 검사 실패, 유효하지 않은 시작 시각은 빈 경로와 사용자용 오류 메시지로 반환한다.
- 저장 동작은 로컬 파일 시스템만 사용하며 네트워크 또는 외부 AI API를 호출하지 않는다.

## 사용 예시

```cpp
MeetingStorage storage(QStringLiteral("meetings"));
const MeetingStorage::Result result = storage.createMeeting(
    QStringLiteral("BMS 개발 주간 회의"));

if (!result.succeeded()) {
    // result.errorMessage를 사용자에게 표시한다.
    return;
}

// result.paths.wav
// result.paths.transcript
// result.paths.json
// result.paths.markdown
```

경로만 예약하며 네 개의 내용 파일은 만들지 않는다. 각 후속 기능이 해당 경로에 WAV, transcript 및 회의록을 기록한다.

## 테스트

`tests/tst_appshell.cpp`에 다음 사례를 추가했다.

- 동일 타임스탬프와 제목으로 두 번 생성했을 때 서로 다른 폴더 및 표준 파일 경로 반환
- 디렉터리 대신 일반 파일을 저장 루트로 지정했을 때 오류 반환
- 제목에 `/`, `\\`, `:`, `?`, `*` 및 `..`가 포함되어도 저장 루트 밖으로 벗어나지 않음
- 유효하지 않은 시작 시각에 오류를 반환하고 폴더를 만들지 않음

빌드와 실행 절차는 `agent-rules/BUILD_GUIDE.md`를 따르며, Qt 6 MSVC 2022 64-bit 환경에서 앱과 테스트 프로젝트를 각각 qmake로 구성한다.

## 검증 결과

Windows 11, Qt 6.11.0 MSVC 2022 64-bit 환경에서 다음을 확인했다.

- 앱 Debug 빌드 성공
- 앱 Release 빌드 성공
- Debug 앱 프로세스 기동 성공
- Qt Test 7건 통과, 실패 및 건너뜀 없음
