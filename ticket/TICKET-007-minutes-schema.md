# TICKET-007: 구조화 회의록 스키마

## Metadata

- Type: Feature
- Size: Small
- Status: TODO

## Goal

LLM 회의록 JSON을 검증 가능한 자료형으로 정의한다.

## Background

`PROJECT_GOAL.md`의 Windows 로컬 회의록 MVP를 단계적으로 구현한다.

## Source

- `PROJECT_GOAL.md` — 5.6 회의록 데이터 구조; 6. 데이터 Schema

## Requirements

- LLM 회의록 JSON을 검증 가능한 자료형으로 정의한다.
- 회의 음성 및 내용은 외부 AI API로 보내지 않는다.

## Implementation Scope

- title/date/summary/topics/decisions/action_items/open_issues 필드와 nullable 담당자·기한을 검증한다.

## Out of Scope

- `PROJECT_GOAL.md` §20 MVP 제외 기능과 다른 티켓의 주요 결과물.

## Dependencies

TICKET-005

## Acceptance Criteria

- [ ] LLM 회의록 JSON을 검증 가능한 자료형으로 정의한다.
- [ ] 예시 JSON 통과
- [ ] 이 티켓의 오류 및 경계 조건을 확인할 수 있다.

## Test Requirements

### Normal Cases

- 예시 JSON 통과

### Error Cases

- 필수 필드 누락 거부

### Edge Cases

- 담당자와 기한 null 허용.

## Files

### Existing

- `PROJECT_GOAL.md` (요구사항; 구현 소스는 제공되지 않음)

### To Create

- 이 티켓 범위의 앱 또는 백엔드 소스 및 필요한 테스트 (경로는 저장소 구조 확인 후 결정)

## Open Questions

None
