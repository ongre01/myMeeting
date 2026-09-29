# TICKET-012: Markdown TXT JSON 내보내기

## Metadata

- Type: Feature
- Size: Medium
- Status: TODO

## Goal

편집한 회의록을 세 형식으로 로컬 저장한다.

## Background

`PROJECT_GOAL.md`의 Windows 로컬 회의록 MVP를 단계적으로 구현한다.

## Source

- `PROJECT_GOAL.md` — 14. Markdown 회의록 형식; 20. MVP 개발 범위

## Requirements

- 편집한 회의록을 세 형식으로 로컬 저장한다.
- 회의 음성 및 내용은 외부 AI API로 보내지 않는다.

## Implementation Scope

- 현재 모델 기준 Markdown, TXT, JSON 직렬화 및 저장 UI를 구현한다.

## Out of Scope

- `PROJECT_GOAL.md` §20 MVP 제외 기능과 다른 티켓의 주요 결과물.

## Dependencies

TICKET-011

## Acceptance Criteria

- [ ] 편집한 회의록을 세 형식으로 로컬 저장한다.
- [ ] 세 파일 재열기 시 수정 내용 보존
- [ ] 이 티켓의 오류 및 경계 조건을 확인할 수 있다.

## Test Requirements

### Normal Cases

- 세 파일 재열기 시 수정 내용 보존

### Error Cases

- 저장 실패 표시

### Edge Cases

- 빈 항목과 특수문자 처리.

## Files

### Existing

- `PROJECT_GOAL.md` (요구사항; 구현 소스는 제공되지 않음)

### To Create

- 이 티켓 범위의 앱 또는 백엔드 소스 및 필요한 테스트 (경로는 저장소 구조 확인 후 결정)

## Open Questions

None
