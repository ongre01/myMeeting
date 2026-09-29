# TICKET-002: 회의별 로컬 저장소

## Metadata

- Type: Feature
- Size: Small
- Status: TODO

## Goal

회의별 디렉터리와 파일 경로를 충돌 없이 생성한다.

## Background

`PROJECT_GOAL.md`의 Windows 로컬 회의록 MVP를 단계적으로 구현한다.

## Source

- `PROJECT_GOAL.md` — 5.2 녹음 파일 관리; 13. 파일 저장 구조

## Requirements

- 회의별 디렉터리와 파일 경로를 충돌 없이 생성한다.
- 회의 음성 및 내용은 외부 AI API로 보내지 않는다.

## Implementation Scope

- 타임스탬프 기반 회의 폴더를 생성하고 WAV, transcript.txt, meeting.json, meeting.md 경로를 제공한다.

## Out of Scope

- `PROJECT_GOAL.md` §20 MVP 제외 기능과 다른 티켓의 주요 결과물.

## Dependencies

TICKET-001

## Acceptance Criteria

- [ ] 회의별 디렉터리와 파일 경로를 충돌 없이 생성한다.
- [ ] 연속 생성 시 경로가 중복되지 않음
- [ ] 이 티켓의 오류 및 경계 조건을 확인할 수 있다.

## Test Requirements

### Normal Cases

- 연속 생성 시 경로가 중복되지 않음

### Error Cases

- 쓰기 불가 위치에서 오류 반환

### Edge Cases

- 제목에 경로 구분자가 있어도 탈출하지 않음.

## Files

### Existing

- `PROJECT_GOAL.md` (요구사항; 구현 소스는 제공되지 않음)

### To Create

- 이 티켓 범위의 앱 또는 백엔드 소스 및 필요한 테스트 (경로는 저장소 구조 확인 후 결정)

## Open Questions

None
