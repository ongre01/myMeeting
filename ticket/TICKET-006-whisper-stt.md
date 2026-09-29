# TICKET-006: NobodyWho Whisper 음성 인식

## Metadata

- Type: Feature
- Size: Medium
- Status: TODO

## Goal

로컬 WAV에서 한국어 Transcript를 생성한다.

## Background

`PROJECT_GOAL.md`의 Windows 로컬 회의록 MVP를 단계적으로 구현한다.

## Source

- `PROJECT_GOAL.md` — 5.3 Speech-to-Text; 21. Phase 2

## Requirements

- 로컬 WAV에서 한국어 Transcript를 생성한다.
- 회의 음성 및 내용은 외부 AI API로 보내지 않는다.

## Implementation Scope

- NobodyWho STT 연동, 필요한 오디오 형식 변환, transcript.txt 저장을 구현한다.

## Out of Scope

- `PROJECT_GOAL.md` §20 MVP 제외 기능과 다른 티켓의 주요 결과물.

## Dependencies

TICKET-005

## Acceptance Criteria

- [ ] 로컬 WAV에서 한국어 Transcript를 생성한다.
- [ ] 한국어 샘플 WAV의 텍스트 출력
- [ ] 이 티켓의 오류 및 경계 조건을 확인할 수 있다.

## Test Requirements

### Normal Cases

- 한국어 샘플 WAV의 텍스트 출력

### Error Cases

- 손상 WAV와 모델 로딩 오류 처리

### Edge Cases

- 짧은/무음 입력 처리.

## Files

### Existing

- `PROJECT_GOAL.md` (요구사항; 구현 소스는 제공되지 않음)

### To Create

- 이 티켓 범위의 앱 또는 백엔드 소스 및 필요한 테스트 (경로는 저장소 구조 확인 후 결정)

## Open Questions

- NobodyWho의 Python STT/LLM 호출 방식과 모델 식별자는 실제 사용 버전을 확인한 후 고정합니다.
