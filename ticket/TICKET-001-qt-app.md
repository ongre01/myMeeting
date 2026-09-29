# TICKET-001: Windows Qt 앱 골격

## Metadata

- Type: Feature
- Size: Small
- Status: TODO

## Goal

Windows 10/11에서 Qt 6 Widgets 앱의 빈 메인 화면을 빌드하고 실행한다.

## Background

`PROJECT_GOAL.md`의 Windows 로컬 회의록 MVP를 단계적으로 구현한다.

## Source

- `PROJECT_GOAL.md` — 개발 환경; 9. Qt UI 사양

## Requirements

- Windows 10/11에서 Qt 6 Widgets 앱의 빈 메인 화면을 빌드하고 실행한다.
- 회의 음성 및 내용은 외부 AI API로 보내지 않는다.

## Implementation Scope

- Qt/C++17 빌드 설정과 앱 진입점, 기본 MainWindow를 구성한다.

## Out of Scope

- `PROJECT_GOAL.md` §20 MVP 제외 기능과 다른 티켓의 주요 결과물.

## Dependencies

None

## Acceptance Criteria

- [ ] Windows 10/11에서 Qt 6 Widgets 앱의 빈 메인 화면을 빌드하고 실행한다.
- [ ] Qt 6 설치 환경에서 Debug 빌드 및 실행 확인
- [ ] 이 티켓의 오류 및 경계 조건을 확인할 수 있다.

## Test Requirements

### Normal Cases

- Qt 6 설치 환경에서 Debug 빌드 및 실행 확인

### Error Cases

- Qt 누락 시 빌드 오류를 확인한다.

### Edge Cases

- 빈 입력과 특수문자 경계를 확인한다.

## Files

### Existing

- `PROJECT_GOAL.md` (요구사항; 구현 소스는 제공되지 않음)

### To Create

- 이 티켓 범위의 앱 또는 백엔드 소스 및 필요한 테스트 (경로는 저장소 구조 확인 후 결정)

## Open Questions

None
