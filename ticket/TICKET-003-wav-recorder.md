# TICKET-003: 마이크 선택과 WAV 녹음

## Metadata

- Type: Feature
- Size: Medium
- Status: TODO

## Goal

선택한 마이크에서 녹음하여 재생 가능한 WAV 파일을 만든다.

## Background

`PROJECT_GOAL.md`의 Windows 로컬 회의록 MVP를 단계적으로 구현한다.

## Source

- `PROJECT_GOAL.md` — 5.1 회의 녹음; 20. MVP 개발 범위

## Requirements

- 선택한 마이크에서 녹음하여 재생 가능한 WAV 파일을 만든다.
- 회의 음성 및 내용은 외부 AI API로 보내지 않는다.

## Implementation Scope

- QAudioSource로 장치를 열고 시작/종료, PCM 16-bit mono WAV 헤더와 파일 저장을 구현한다.

## Out of Scope

- `PROJECT_GOAL.md` §20 MVP 제외 기능과 다른 티켓의 주요 결과물.

## Dependencies

TICKET-001, TICKET-002

## Acceptance Criteria

- [ ] 선택한 마이크에서 녹음하여 재생 가능한 WAV 파일을 만든다.
- [ ] 장치 선택 후 녹음한 파일을 재생 및 헤더 검사
- [ ] 이 티켓의 오류 및 경계 조건을 확인할 수 있다.

## Test Requirements

### Normal Cases

- 장치 선택 후 녹음한 파일을 재생 및 헤더 검사

### Error Cases

- 장치 없음과 저장 실패 표시

### Edge Cases

- 0초 녹음 처리.

## Files

### Existing

- `PROJECT_GOAL.md` (요구사항; 구현 소스는 제공되지 않음)

### To Create

- 이 티켓 범위의 앱 또는 백엔드 소스 및 필요한 테스트 (경로는 저장소 구조 확인 후 결정)

## Open Questions

- 녹음 일시정지·재개는 §5.1에는 필수이나 §20 MVP 필수 목록에는 빠져 있습니다. MVP에 포함할지 결정이 필요합니다.
