# TICKET-008: 로컬 LLM 회의록 분석

## Metadata

- Type: Feature
- Size: Medium
- Status: TODO

## Goal

수정 가능한 Transcript에서 검증된 meeting.json을 생성한다.

## Background

`PROJECT_GOAL.md`의 Windows 로컬 회의록 MVP를 단계적으로 구현한다.

## Source

- `PROJECT_GOAL.md` — 5.5 AI 회의록 생성; 7. AI Prompt 기본 사양; 21. Phase 3

## Requirements

- 수정 가능한 Transcript에서 검증된 meeting.json을 생성한다.
- 회의 음성 및 내용은 외부 AI API로 보내지 않는다.

## Implementation Scope

- NobodyWho GGUF LLM 호출, 근거 없는 사실 금지 프롬프트, 스키마 검증 실패 처리와 로컬 JSON 저장을 구현한다.

## Out of Scope

- `PROJECT_GOAL.md` §20 MVP 제외 기능과 다른 티켓의 주요 결과물.

## Dependencies

TICKET-007

## Acceptance Criteria

- [ ] 수정 가능한 Transcript에서 검증된 meeting.json을 생성한다.
- [ ] 샘플 Transcript 요약·결정·Action Item 추출
- [ ] 이 티켓의 오류 및 경계 조건을 확인할 수 있다.

## Test Requirements

### Normal Cases

- 샘플 Transcript 요약·결정·Action Item 추출

### Error Cases

- 잘못된 JSON 오류 처리

### Edge Cases

- 불명확한 담당자·기한은 null.

## Files

### Existing

- `PROJECT_GOAL.md` (요구사항; 구현 소스는 제공되지 않음)

### To Create

- 이 티켓 범위의 앱 또는 백엔드 소스 및 필요한 테스트 (경로는 저장소 구조 확인 후 결정)

## Open Questions

- NobodyWho의 Python STT/LLM 호출 방식과 모델 식별자는 실제 사용 버전을 확인한 후 고정합니다.
