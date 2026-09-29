# TICKET-010: Transcript 표시와 편집

## Metadata

- Type: Feature
- Size: Medium
- Status: TODO

## Goal

사용자가 Transcript를 수정하고 분석 입력으로 사용할 수 있다.

## Background

`PROJECT_GOAL.md`의 Windows 로컬 회의록 MVP를 단계적으로 구현한다.

## Source

- `PROJECT_GOAL.md` — 5.4 Transcript 관리; 20. MVP 개발 범위

## Requirements

- 사용자가 Transcript를 수정하고 분석 입력으로 사용할 수 있다.
- 회의 음성 및 내용은 외부 AI API로 보내지 않는다.

## Implementation Scope

- 텍스트 표시·편집, transcript.txt 저장, 편집된 내용의 분석 재실행 경로를 만든다.

## Out of Scope

- `PROJECT_GOAL.md` §20 MVP 제외 기능과 다른 티켓의 주요 결과물.

## Dependencies

TICKET-009

## Acceptance Criteria

- [ ] 사용자가 Transcript를 수정하고 분석 입력으로 사용할 수 있다.
- [ ] 전문 용어 수정 저장 후 재열기
- [ ] 이 티켓의 오류 및 경계 조건을 확인할 수 있다.

## Test Requirements

### Normal Cases

- 전문 용어 수정 저장 후 재열기

### Error Cases

- 빈 Transcript 분석 차단

### Edge Cases

- 편집 후 재분석에 수정본 사용.

## Files

### Existing

- `PROJECT_GOAL.md` (요구사항; 구현 소스는 제공되지 않음)

### To Create

- 이 티켓 범위의 앱 또는 백엔드 소스 및 필요한 테스트 (경로는 저장소 구조 확인 후 결정)

## Open Questions

None
