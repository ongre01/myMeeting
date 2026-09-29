# TICKET-013: 로컬 로그와 통합 검증

## Metadata

- Type: Feature
- Size: Medium
- Status: TODO

## Goal

종단 간 워크플로와 내용 비기록 로그 원칙을 확인한다.

## Background

`PROJECT_GOAL.md`의 Windows 로컬 회의록 MVP를 단계적으로 구현한다.

## Source

- `PROJECT_GOAL.md` — 17. 개인정보 및 보안; 18. 오류 처리; 19. 로그; 23. 최종 목표

## Requirements

- 종단 간 워크플로와 내용 비기록 로그 원칙을 확인한다.
- 회의 음성 및 내용은 외부 AI API로 보내지 않는다.

## Implementation Scope

- 시작/종료·작업 시간·오류 로그를 만들고 녹음부터 세 형식 저장까지 수동 통합 절차를 문서화한다.

## Out of Scope

- `PROJECT_GOAL.md` §20 MVP 제외 기능과 다른 티켓의 주요 결과물.

## Dependencies

TICKET-012

## Acceptance Criteria

- [ ] 종단 간 워크플로와 내용 비기록 로그 원칙을 확인한다.
- [ ] 정상 워크플로 완료
- [ ] 이 티켓의 오류 및 경계 조건을 확인할 수 있다.

## Test Requirements

### Normal Cases

- 정상 워크플로 완료

### Error Cases

- 모델 실패 복구 확인

### Edge Cases

- 로그에 음성·Transcript·LLM 원문 없음.

## Files

### Existing

- `PROJECT_GOAL.md` (요구사항; 구현 소스는 제공되지 않음)

### To Create

- 이 티켓 범위의 앱 또는 백엔드 소스 및 필요한 테스트 (경로는 저장소 구조 확인 후 결정)

## Open Questions

None
