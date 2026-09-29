# TICKET-003 마이크 선택과 WAV 녹음 구현

## 구현 범위

- `AudioRecorder`가 `QMediaDevices::audioInputs()`로 현재 입력 장치 목록을 제공한다.
- 호출자가 목록에서 선택한 `QAudioDevice`와 저장 경로를 `startRecording()`에 전달하면 해당 장치로 `QAudioSource`를 시작한다.
- 녹음 형식은 16-bit mono PCM이며 48 kHz를 우선하고, 장치가 지원하지 않으면 16 kHz를 사용한다. 두 형식을 모두 지원하지 않는 장치는 사용자용 오류로 거부한다.
- `WavFileWriter`가 녹음 시작 시 44-byte RIFF/WAVE 헤더를 만들고 PCM 데이터를 순차 저장한다. 녹음 종료 시 RIFF 및 data chunk 크기를 실제 PCM 바이트 수로 갱신한다.
- 녹음을 즉시 종료해 PCM 데이터가 없어도 data 크기가 0인 유효한 WAV 파일을 남긴다.
- 장치 미선택, 지원 형식 없음, 마이크 열기/읽기 실패, 저장 파일 열기/쓰기/완료 실패를 `recordingError(QString)`로 전달한다.
- WAV의 32-bit chunk 크기 한계에 맞춰 PCM 데이터가 허용 크기를 넘으면 쓰기를 중단한다.
- 녹음 데이터는 지정된 로컬 파일에만 기록하며 네트워크 또는 외부 AI API를 사용하지 않는다.

일시정지·재개와 녹음 버튼, 제목 입력, 장치 콤보박스, 경과 시간 표시는 TICKET-004 범위이므로 이번 구현에 포함하지 않았다.

## 주요 API

```cpp
AudioRecorder recorder;
const QList<QAudioDevice> devices = AudioRecorder::availableInputDevices();

if (!devices.isEmpty()) {
    recorder.startRecording(devices.first(), meetingPaths.wav);
    // ...
    recorder.stopRecording();
}
```

정상 시작 시 `recordingStarted()`, 정상 종료 및 헤더 완료 시 `recordingStopped(filePath)`가 발생한다. 시작 또는 녹음 중 실패하면 `recordingError(message)`가 발생하며, 실패 후 내부 상태를 초기화하므로 다시 시작할 수 있다.

## 자동 테스트

`tests/tst_appshell.cpp`에 다음 검증을 추가했다.

- 알려진 PCM 샘플을 기록한 뒤 RIFF, WAVE, fmt, data 식별자와 little-endian chunk 크기, PCM 형식 코드, 채널 수, sample rate, byte rate, block align, bits per sample 및 payload 확인
- 0초 녹음에 해당하는 44-byte WAV 파일과 0-byte data chunk 확인
- 존재하지 않는 저장 디렉터리에 파일을 열 때 오류 메시지 반환 확인
- null `QAudioDevice`로 녹음을 시작할 때 `recordingError` 발생 및 정지 상태 유지 확인

Windows 11, Qt 6.11.0 MSVC 2022 64-bit 환경에서 다음을 확인했다.

- 앱 Debug 및 Release 빌드 성공
- 테스트 Debug 및 Release 빌드 성공
- Debug/Release 각각 Qt Test 11건 통과, 실패 및 건너뜀 없음
- Debug 앱 프로세스 기동 확인

자동 테스트에서는 사용자 동의 없이 실제 마이크를 켜지 않는다. 따라서 물리 장치에서 녹음한 음성의 청취 검증은 아래 절차로 별도 수행해야 한다.

## 실제 마이크 수동 확인

TICKET-004에서 UI가 연결된 뒤 다음 순서로 확인한다.

1. 장치 목록에서 사용할 마이크를 선택한다.
2. 녹음을 시작하고 짧게 말한 다음 종료한다.
3. 생성된 `meeting.wav`를 Windows 미디어 플레이어에서 재생해 녹음 내용을 확인한다.
4. 파일 속성 또는 WAV 검사 도구에서 PCM, mono, 16-bit, 16 kHz 또는 48 kHz인지 확인한다.
5. 시작 직후 종료해도 재생기가 파일을 열고 재생 시간이 0초로 표시되는지 확인한다.
6. 마이크를 비활성화한 경우와 쓸 수 없는 저장 경로에서 사용자용 오류가 표시되는지 확인한다.
