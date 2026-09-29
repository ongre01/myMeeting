# TICKET-004: 녹음 화면과 상태 표시

## Metadata

- Type: Feature
- Size: Medium
- Status: TODO

## Goal

GUI에서 장치를 선택하고 녹음 상태와 경과 시간을 확인한다.

## Background

`PROJECT_GOAL.md`의 Windows 로컬 회의록 MVP를 단계적으로 구현한다.

## Source

- `PROJECT_GOAL.md` — 9. Qt UI 사양; 12. 처리 상태

## Requirements

- GUI에서 장치를 선택하고 녹음 상태와 경과 시간을 확인한다.
- 회의 음성 및 내용은 외부 AI API로 보내지 않는다.

## Implementation Scope

- 녹음 시작/종료 버튼, 제목 입력, 장치 목록과 상태 표시를 AudioRecorder에 연결한다.

## Out of Scope

- `PROJECT_GOAL.md` §20 MVP 제외 기능과 다른 티켓의 주요 결과물.

## Dependencies

TICKET-003

## Acceptance Criteria

- [ ] GUI에서 장치를 선택하고 녹음 상태와 경과 시간을 확인한다.
- [ ] 버튼으로 WAV 생성
- [ ] 이 티켓의 오류 및 경계 조건을 확인할 수 있다.

## Test Requirements

### Normal Cases

- 버튼으로 WAV 생성

### Error Cases

- 녹음 중 중복 시작 방지

### Edge Cases

- 실패 후 다시 시작 가능.

## Files

### Existing

- `PROJECT_GOAL.md` (요구사항; 구현 소스는 제공되지 않음)

### To Create

- 이 티켓 범위의 앱 또는 백엔드 소스 및 필요한 테스트 (경로는 저장소 구조 확인 후 결정)

## Open Questions

None
