# TICKET-001 Qt 앱 골격 구현

## 구현 범위

- `myMeeting.pro`에 Qt 6 Widgets, C++17, Windows 전용 앱 구성을 명시했다.
- `main.cpp`에서 애플리케이션 식별 정보를 설정하고 빈 `MainWindow`를 표시한다.
- 메인 창 제목은 `Local Meeting Minutes Assistant`이며 중앙 위젯에는 아직 기능성 컨트롤이 없다.
- `tests/tst_appshell.cpp`에서 창 제목, 빈 중앙 위젯, 상태 표시줄 생성을 검증한다.
- 이 골격에는 네트워크 또는 외부 AI API 호출 코드가 없다.

## 빌드 및 실행

Qt 6 MSVC 2022 64-bit와 Visual Studio 2022 Developer PowerShell을 사용한다.

```powershell
mkdir build-debug
cd build-debug
C:\Qt\6.11.0\msvc2022_64\bin\qmake.exe ..\myMeeting.pro CONFIG+=debug
nmake
$env:Path = "C:\Qt\6.11.0\msvc2022_64\bin;$env:Path"
.\debug\myMeeting.exe
```

Qt가 다른 위치에 설치된 경우 `qmake.exe` 경로만 해당 설치 경로로 바꾼다. Qt 6이 아닌 qmake로 프로젝트를 구성하면 `myMeeting requires Qt 6.x.` 오류와 함께 중단된다. qmake 자체가 설치되지 않았거나 PATH 및 명시 경로에서 찾을 수 없으면 구성 단계가 시작되지 않으므로 Qt 6을 먼저 설치해야 한다.

## 테스트

```powershell
mkdir build-tests-debug
cd build-tests-debug
C:\Qt\6.11.0\msvc2022_64\bin\qmake.exe ..\tests\tests.pro CONFIG+=debug
nmake
$env:Path = "C:\Qt\6.11.0\msvc2022_64\bin;$env:Path"
.\debug\tst_appshell.exe
```

현재 티켓의 UI는 사용자 입력을 받지 않으므로 빈 입력과 입력 특수문자 처리 경로는 존재하지 않는다. 빈 문자열 및 한글·공백·기호가 포함된 추가 명령줄 인수는 앱 기능에 사용하지 않고 무시하며, 해당 인수로도 앱이 정상 기동됨을 확인한다. 창 제목 같은 정적 문자열은 Qt의 UTF-8 XML 및 `QStringLiteral`로 보존된다. 실제 입력 컨트롤의 빈 값과 특수문자 동작은 해당 컨트롤이 도입되는 후속 티켓에서 검증한다.

## 구현 검증 결과

Windows 11, Qt 6.11.0 MSVC 2022 64-bit 환경에서 다음을 확인했다.

- Debug 및 Release 빌드 성공
- Debug 앱 프로세스 기동 성공
- 빈 문자열과 한글·공백·기호가 포함된 추가 명령줄 인수로 기동 성공
- Qt Test 3건 통과, 실패 없음
- Qt 5.15.2 qmake 사용 시 `myMeeting requires Qt 6.x.` 오류와 종료 코드 3 확인
