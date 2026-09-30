# TICKET-006 NobodyWho Whisper 음성 인식 구현

> 현재 CLI는 후속 TICKET-008 구현까지 연결되어 비무음 WAV를 STT한 뒤 Local LLM 분석과 `meeting.json` 저장도 수행한다. 아래 내용은 TICKET-006에서 추가한 STT 단계의 동작을 설명한다.

## 구현 범위

- `backend/stt.py`에 비압축 PCM/IEEE float WAV 검증, mono 16-bit 정규화, NobodyWho Whisper 호출, UTF-8 Transcript 저장을 구현했다.
- `backend/main.py`가 입력과 설정을 검증한 뒤 STT를 실행하고 `transcript.txt`를 저장한다.
- NobodyWho Python API는 3.0.0 기준 `SpeechToText(source=..., language=...)`와 `transcribe_file(...).completed()`를 우선 사용한다. 파일 API가 없는 테스트/호환 어댑터는 `transcribe_pcm(...)`으로 대체한다.
- 설정의 `ko-KR` 같은 지역 언어 태그는 NobodyWho가 요구하는 ISO 639-1 기본 코드 `ko`로 정규화한다.
- Python 의존성은 재현 가능한 설치를 위해 `nobodywho==3.0.0`으로 고정했다.
- 회의 음성이나 Transcript를 외부 AI API로 전송하는 코드는 없다. `hf://` 모델을 처음 사용할 때는 모델 파일만 Hugging Face에서 내려받아 로컬 캐시에 저장한다.

## WAV 처리

입력은 비압축 PCM 또는 IEEE float RIFF/WAV여야 한다. 표준 포맷 태그와 `WAVE_FORMAT_EXTENSIBLE` 서브포맷 GUID, RIFF 청크 경계, 실제 오디오 길이를 확인한다. NobodyWho 3.0의 파일 API가 있으면 검증한 WAV 경로를 직접 전달해 장시간 녹음을 거대한 Python 정수 리스트로 만들지 않는다. 파일 API가 없는 호환 어댑터에서는 다음 규칙으로 mono signed 16-bit PCM을 생성한다.

| 입력 | 처리 |
|---|---|
| PCM 8/16/24/32-bit | signed 16-bit 범위로 변환 |
| IEEE float 32/64-bit | `[-1.0, 1.0]`을 signed 16-bit 범위로 변환 |
| `WAVE_FORMAT_EXTENSIBLE` PCM/IEEE float | 서브포맷 GUID를 해석해 해당 PCM/float 규칙 적용 |
| 다채널 | 채널 평균으로 mono downmix |
| 임의 샘플레이트 | 원래 샘플레이트와 PCM을 NobodyWho에 전달; Whisper 입력 리샘플링은 NobodyWho가 수행 |
| 빈 WAV 또는 완전한 디지털 무음 | 모델을 로드하지 않고 빈 Transcript 저장 |
| 1 프레임 이상의 비무음 WAV | 길이에 상관없이 STT 실행 |

손상된 헤더, 잘린 오디오 데이터, 압축 WAV, RF64, 지원하지 않는 샘플 폭이나 extensible 서브포맷은 입력 오류로 처리한다. Python 스택 트레이스는 CLI에 노출하지 않는다.

## 실행 방법

저장소 루트에서 가상 환경을 준비하고 의존성을 설치한다.

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -r backend\requirements.txt
```

기본 실행 명령은 다음과 같다.

```powershell
python backend\main.py `
    --input recordings\meeting.wav `
    --output meetings\meeting.json
```

`--output`은 로컬 LLM 단계가 생성할 `meeting.json` 경로다. 같은 폴더의 `transcript.txt`를 기본 Transcript 경로로 사용한다. 경로를 직접 지정하려면 다음 인수를 추가한다.

```powershell
--transcript-output meetings\2026-09-30_103000\transcript.txt
```

TICKET-006 단계의 STT 성공 결과는 표준 출력에 다음 형태의 한 줄 JSON을 기록한다. 현재 비무음 전체 CLI 성공 결과는 TICKET-008 문서의 `status: "completed"` 형식을 사용한다.

```json
{
  "status": "transcribed",
  "input": "<절대 WAV 경로>",
  "output": "<절대 meeting.json 경로>",
  "transcript_output": "<절대 transcript.txt 경로>",
  "config": "<절대 설정 경로>",
  "language": "ko",
  "characters": 31,
  "audio_duration_seconds": 5.653,
  "silence": false
}
```

Transcript는 UTF-8과 LF 줄바꿈으로 저장한다. 임시 파일을 같은 폴더에 완전히 쓴 뒤 원자적으로 교체하므로 저장 실패 시 기존 Transcript를 보존한다.

## 모델 설정

기본 `backend/config.json`은 다음 STT 모델과 한국어 코드를 사용한다.

```json
{
  "stt_model": "hf://onnx-community/whisper-base",
  "language": "ko"
}
```

NobodyWho 3.0은 ONNX Whisper 모델만 지원한다. `stt_model`에는 `hf://owner/repo` 또는 같은 구조의 로컬 모델 폴더를 지정할 수 있다. 완전한 오프라인 실행이 필요하면 모델을 미리 캐시에 내려받거나 로컬 폴더 경로로 설정한다.

공식 API 근거:

- [NobodyWho Python Speech to Text](https://docs.nobodywho.ooo/python/speech-to-text/)
- [NobodyWho 3.0.0 on PyPI](https://pypi.org/project/nobodywho/3.0.0/)

## 오류와 종료 코드

| 종료 코드 | 상황 |
|---:|---|
| 3 | 손상·잘림·미지원 형식 등 입력 WAV 오류 |
| 4 | Transcript 경로/권한/원자 저장 오류 |
| 5 | 모델 설정 파일 또는 설정값 오류 |
| 6 | NobodyWho 미설치, Whisper 모델 로딩 실패, 추론 실패 |

모델 로딩 또는 추론 실패 시 `transcript.txt`를 새로 쓰지 않는다. 무음은 실패가 아니며 종료 코드 0, `silence: true`, 빈 Transcript로 반환한다.

## 자동 검증

```powershell
python -m unittest discover -s backend\tests -v
python -m compileall -q backend
```

테스트는 다음을 포함한다.

- 한국어 문자열을 반환하는 NobodyWho 호환 어댑터를 통한 CLI 전체 경로와 UTF-8 파일 저장
- 8/16/24/32-bit PCM과 32-bit IEEE float 변환, `WAVE_FORMAT_EXTENSIBLE` float 해석, stereo downmix, 48 kHz 전달
- NobodyWho 파일 전사 API 우선 사용과 PCM-only 호환 어댑터 fallback
- 1 프레임 비무음 입력과 빈/무음 입력
- 손상 및 잘린 WAV
- 모델 로딩과 추론 오류, 비문자열 결과
- Transcript 원자 저장 실패 시 기존 파일 보존
- 기존 TICKET-005 경로·설정·종료 코드 회귀 테스트

## 검증 결과

2026-09-30 Windows 환경에서 다음을 확인했다.

- Python 3.12, 3.13, 3.14에서 백엔드 `unittest` 각각 27건 통과
- Python 3.14.7에서 `compileall` 성공
- 격리 설치한 NobodyWho 3.0.0에서 실제 `SpeechToText(source, language, quantization)` 및 `transcribe_pcm(samples, sample_rate)` API 로딩 성공
- 로컬 `ko-KR` 합성 음성 5.653초를 기본 `whisper-base`, `language="ko"`로 실제 처리해 `오늘 회의를 시작합니다. 캔 통신 문제를 확인하겠습니다.` 출력 및 UTF-8 `transcript.txt` 저장 성공
- GPU provider를 사용할 수 없는 환경에서 CUDA 등록 경고 후 CPU fallback으로 정상 완료
- Qt 6.11.0 MSVC 2022 64-bit 앱 Debug/Release 빌드 성공
- Qt Test Debug/Release 모두 종료 코드 0
- Debug/Release 앱이 즉시 종료하지 않고 이벤트 루프에 진입함

실제 종단 검증에 사용한 합성 WAV, 임시 패키지 복사본, 출력 파일은 검증 후 삭제했다. NobodyWho가 사용하는 모델 캐시는 임시 검증 파일 정리 대상에 포함하지 않았다.

### 저장된 float WAV 호환성 보강

2026-09-30 실제 48 kHz mono, 32-bit IEEE float, `WAVE_FORMAT_EXTENSIBLE` WAV에서 다음을 추가 확인했다.

- 원본과 앱이 회의 폴더로 복사한 `meeting.wav`의 SHA-256 일치
- 4,806.164초/230,695,872 프레임을 전체 PCM 리스트로 읽지 않고 헤더와 비무음 상태 확인
- 캐시된 NobodyWho 3.0.0 `whisper-base`와 짧은 로컬 float32 WAV를 사용한 `transcribe_file()` 성공
- Python `unittest` 49건 통과 및 `compileall` 성공

검증용 짧은 WAV는 확인 후 삭제했으며 전사 내용은 로그나 문서에 기록하지 않았다.
