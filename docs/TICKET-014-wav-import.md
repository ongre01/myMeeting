# TICKET-014 저장된 WAV 파일 불러오기

## 구현 범위

- 메인 화면에 `WAV 불러오기` 버튼과 선택 경로 표시를 추가했다.
- 사용자가 선택한 `.wav` 파일을 새 회의 폴더의 `meeting.wav`로 복사한다.
- 불러오기가 끝나면 기존 `AI 회의록 생성` 버튼을 활성화하고, 기존과 동일한 로컬 STT→LLM 처리를 실행한다.
- 녹음 또는 AI 처리 중에는 다른 WAV 파일을 불러올 수 없도록 버튼을 비활성화한다.

## 파일 처리 정책

선택한 원본 WAV 파일 옆에 출력을 쓰지 않는다. 현재 시각과 회의 제목(없으면 원본 파일명)으로 새 회의 폴더를 만들고 다음 구조로 저장한다.

```text
meetings/<timestamp>_<title>/
  meeting.wav       # 선택한 원본의 로컬 복사본
  transcript.txt    # STT 결과
  meeting.json      # Local LLM 회의록
```

이 구조는 원본 WAV를 보존하고 기존 녹음 회의와 같은 저장·편집·내보내기 흐름을 사용하게 한다. 로컬 파일과 로컬 Python 백엔드만 사용하며 외부 업로드를 추가하지 않았다.

## 오류 처리

- 존재하지 않는 경로, 일반 파일이 아닌 경로, `.wav` 외 확장자, 읽기 불가능 파일을 UI 단계에서 거부한다.
- 회의 폴더 생성이나 WAV 복사가 실패하면 사용자에게 이해할 수 있는 메시지를 표시한다.
- 복사 실패 시 해당 시도에서 만든 빈 회의 폴더를 정리한다.
- WAV 내부 포맷은 Python STT 검증을 사용한다. PCM 8/16/24/32-bit, IEEE float 32/64-bit와 두 형식의 `WAVE_FORMAT_EXTENSIBLE` 헤더를 지원하며, 손상되거나 그 밖의 형식인 WAV는 AI 처리 시 기존 오류 흐름으로 보고된다.

## 검증 범위

자동 테스트는 임시 저장된 WAV를 불러와 새 회의 폴더로 바이트 단위 복사되는지, 원본이 바뀌지 않는지, 이후 로컬 fake 백엔드로 `transcript.txt`와 `meeting.json`을 생성할 수 있는지를 확인한다. 또한 `.wav` 외 확장자 거부와 녹음·AI 처리 중 불러오기 비활성화를 검증한다.

2026-09-30 Windows 11, Qt 6.11.0 MSVC 2022 64-bit 환경에서 다음을 확인했다.

- Qt 앱 Debug/Release 빌드 성공
- Qt Test Debug/Release 각각 32건 통과, 실패 및 건너뜀 0건
- Python `unittest` 49건 통과 및 `compileall` 성공
- Debug/Release 앱이 offscreen 환경에서 즉시 종료하지 않고 이벤트 루프에 진입

실제 NobodyWho Whisper/GGUF 모델의 인식·요약 품질은 로컬 모델 파일과 대상 하드웨어를 준비한 뒤 별도로 확인해야 한다.

추가로 실제 약 80분 길이의 48 kHz mono, 32-bit IEEE float, `WAVE_FORMAT_EXTENSIBLE` WAV를 사용해 원본과 복사본의 SHA-256 일치, 대용량 리스트를 생성하지 않는 파일 전사 경로 선택, 캐시된 NobodyWho `whisper-base`의 짧은 float32 WAV 전사 성공을 확인했다. 검증 중 음성 내용은 출력하거나 로그에 저장하지 않았다.
