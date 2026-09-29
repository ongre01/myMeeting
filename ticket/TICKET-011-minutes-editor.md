# TICKET-011: 회의록 편집 화면

## Metadata

- Type: Feature
- Size: Medium
- Status: TODO

## Goal

구조화 결과를 화면에서 보고 사용자가 수정한다.

## Background

`PROJECT_GOAL.md`의 Windows 로컬 회의록 MVP를 단계적으로 구현한다.

## Source

- `PROJECT_GOAL.md` — 1.2 주요 목표; 9. Qt UI 사양

## Requirements

- 구조화 결과를 화면에서 보고 사용자가 수정한다.
- 회의 음성 및 내용은 외부 AI API로 보내지 않는다.

## Implementation Scope

- 요약·논의·결정·Action Item·미해결 이슈 편집 UI와 메모리 모델 동기화를 구현한다.

## Out of Scope

- `PROJECT_GOAL.md` §20 MVP 제외 기능과 다른 티켓의 주요 결과물.

## Dependencies

TICKET-007, TICKET-009

## Acceptance Criteria

- [ ] 구조화 결과를 화면에서 보고 사용자가 수정한다.
- [ ] 각 필드 수정이 현재 모델에 반영
- [ ] 이 티켓의 오류 및 경계 조건을 확인할 수 있다.

## Test Requirements

### Normal Cases

- 각 필드 수정이 현재 모델에 반영

### Error Cases

- 빈 목록 표시

### Edge Cases

- 잘못된 JSON 로드 시 오류.

## Files

### Existing

- `PROJECT_GOAL.md` (요구사항; 구현 소스는 제공되지 않음)

### To Create

- 이 티켓 범위의 앱 또는 백엔드 소스 및 필요한 테스트 (경로는 저장소 구조 확인 후 결정)

## Open Questions

None
