# TICKET-009: Qt와 Python 작업 연동

## Metadata

- Type: Feature
- Size: Medium
- Status: TODO

## Goal

GUI가 백엔드 작업을 비동기로 실행하고 결과와 오류를 수신한다.

## Background

`PROJECT_GOAL.md`의 Windows 로컬 회의록 MVP를 단계적으로 구현한다.

## Source

- `PROJECT_GOAL.md` — 10.3 AiBackendClient; 11. Python Backend 인터페이스; 12. 처리 상태

## Requirements

- GUI가 백엔드 작업을 비동기로 실행하고 결과와 오류를 수신한다.
- 회의 음성 및 내용은 외부 AI API로 보내지 않는다.

## Implementation Scope

- QProcess로 CLI 호출, 종료 코드·출력 파일 검증, Transcribing/Analyzing/Completed/Error 상태와 사용자 메시지를 연결한다.

## Out of Scope

- `PROJECT_GOAL.md` §20 MVP 제외 기능과 다른 티켓의 주요 결과물.

## Dependencies

TICKET-004, TICKET-006, TICKET-008

## Acceptance Criteria

- [ ] GUI가 백엔드 작업을 비동기로 실행하고 결과와 오류를 수신한다.
- [ ] 성공 결과 UI 전달
- [ ] 이 티켓의 오류 및 경계 조건을 확인할 수 있다.

## Test Requirements

### Normal Cases

- 성공 결과 UI 전달

### Error Cases

- 실행 파일 부재·비정상 종료 오류

### Edge Cases

- 실행 중 중복 요청 방지.

## Files

### Existing

- `PROJECT_GOAL.md` (요구사항; 구현 소스는 제공되지 않음)

### To Create

- 이 티켓 범위의 앱 또는 백엔드 소스 및 필요한 테스트 (경로는 저장소 구조 확인 후 결정)

## Open Questions

None
