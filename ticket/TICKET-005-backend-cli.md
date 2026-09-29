# TICKET-005: 백엔드 CLI와 모델 설정

## Metadata

- Type: Feature
- Size: Small
- Status: TODO

## Goal

로컬 백엔드 CLI가 입력·출력 경로와 모델 설정을 검증한다.

## Background

`PROJECT_GOAL.md`의 Windows 로컬 회의록 MVP를 단계적으로 구현한다.

## Source

- `PROJECT_GOAL.md` — 11. Python Backend 인터페이스; 16. 모델 관리; 17. 개인정보 및 보안

## Requirements

- 로컬 백엔드 CLI가 입력·출력 경로와 모델 설정을 검증한다.
- 회의 음성 및 내용은 외부 AI API로 보내지 않는다.

## Implementation Scope

- Python 실행 환경, 설정 파일, CLI 인수 검증 및 종료 코드와 오류 메시지를 정의한다.

## Out of Scope

- `PROJECT_GOAL.md` §20 MVP 제외 기능과 다른 티켓의 주요 결과물.

## Dependencies

None

## Acceptance Criteria

- [ ] 로컬 백엔드 CLI가 입력·출력 경로와 모델 설정을 검증한다.
- [ ] 유효한 경로로 CLI 진입
- [ ] 이 티켓의 오류 및 경계 조건을 확인할 수 있다.

## Test Requirements

### Normal Cases

- 유효한 경로로 CLI 진입

### Error Cases

- 없는 WAV와 누락된 모델 설정은 실패 코드

### Edge Cases

- 네트워크 API 설정 없이 실행.

## Files

### Existing

- `PROJECT_GOAL.md` (요구사항; 구현 소스는 제공되지 않음)

### To Create

- 이 티켓 범위의 앱 또는 백엔드 소스 및 필요한 테스트 (경로는 저장소 구조 확인 후 결정)

## Open Questions

None
