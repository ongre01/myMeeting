#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMediaDevices>
#include <QStatusBar>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : MainWindow(new AudioRecorder,
                 new AiBackendClient,
                 defaultMeetingsRoot(),
                 true,
                 true,
                 parent)
{
}

MainWindow::MainWindow(AudioRecorder *audioRecorder,
                       const QString &meetingsRoot,
                       QWidget *parent)
    : MainWindow(audioRecorder,
                 new AiBackendClient,
                 meetingsRoot,
                 false,
                 true,
                 parent)
{
}

MainWindow::MainWindow(AudioRecorder *audioRecorder,
                       AiBackendClient *aiBackendClient,
                       const QString &meetingsRoot,
                       QWidget *parent)
    : MainWindow(audioRecorder,
                 aiBackendClient,
                 meetingsRoot,
                 false,
                 false,
                 parent)
{
}

MainWindow::MainWindow(AudioRecorder *audioRecorder,
                       AiBackendClient *aiBackendClient,
                       const QString &meetingsRoot,
                       bool takeRecorderOwnership,
                       bool takeBackendOwnership,
                       QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_audioRecorder(audioRecorder)
    , m_aiBackendClient(aiBackendClient)
    , m_storage(meetingsRoot)
    , m_elapsedTimer(new QTimer(this))
{
    Q_ASSERT(m_audioRecorder);
    Q_ASSERT(m_aiBackendClient);
    if (takeRecorderOwnership) {
        m_audioRecorder->setParent(this);
    }
    if (takeBackendOwnership) {
        m_aiBackendClient->setParent(this);
    }

    ui->setupUi(this);
    initialize();
}

MainWindow::~MainWindow()
{
    delete ui;
}

QString MainWindow::defaultMeetingsRoot()
{
    return QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("meetings"));
}

void MainWindow::initialize()
{
    m_elapsedTimer->setInterval(250);

    connect(ui->refreshDevicesButton, &QPushButton::clicked,
            this, &MainWindow::refreshInputDevices);
    connect(ui->startRecordingButton, &QPushButton::clicked,
            this, &MainWindow::startRecording);
    connect(ui->stopRecordingButton, &QPushButton::clicked,
            this, &MainWindow::stopRecording);
    connect(ui->generateMinutesButton, &QPushButton::clicked,
            this, &MainWindow::startBackendProcessing);
    connect(m_elapsedTimer, &QTimer::timeout,
            this, &MainWindow::updateElapsedTime);
    connect(m_audioRecorder, &AudioRecorder::recordingStarted,
            this, &MainWindow::handleRecordingStarted);
    connect(m_audioRecorder, &AudioRecorder::recordingStopped,
            this, &MainWindow::handleRecordingStopped);
    connect(m_audioRecorder, &AudioRecorder::recordingError,
            this, &MainWindow::handleRecordingError);
    connect(m_aiBackendClient, &AiBackendClient::stateChanged,
            this, &MainWindow::handleBackendStateChanged);
    connect(m_aiBackendClient, &AiBackendClient::completed,
            this, &MainWindow::handleBackendCompleted);
    connect(m_aiBackendClient, &AiBackendClient::failed,
            this, &MainWindow::handleBackendFailed);

    auto *mediaDevices = new QMediaDevices(this);
    connect(mediaDevices, &QMediaDevices::audioInputsChanged,
            this, &MainWindow::refreshInputDevices);

    setRecordingControls(false);
    ui->backendProgressBar->setRange(0, 1);
    ui->backendProgressBar->setValue(0);
    refreshInputDevices();
    statusBar()->showMessage(QStringLiteral("로컬 녹음만 사용합니다."));
}

void MainWindow::refreshInputDevices()
{
    const QByteArray previousDeviceId = ui->deviceComboBox->currentData().toByteArray();
    ui->deviceComboBox->clear();

    const QList<AudioRecorder::InputDeviceInfo> devices =
        m_audioRecorder->inputDevices();
    for (const AudioRecorder::InputDeviceInfo &device : devices) {
        const QString description = device.description.trimmed().isEmpty()
                                        ? QStringLiteral("이름 없는 마이크")
                                        : device.description;
        ui->deviceComboBox->addItem(description, device.id);
    }

    const int previousIndex = ui->deviceComboBox->findData(previousDeviceId);
    if (previousIndex >= 0) {
        ui->deviceComboBox->setCurrentIndex(previousIndex);
    }

    if (devices.isEmpty()) {
        ui->deviceComboBox->addItem(QStringLiteral("사용 가능한 마이크 없음"));
        ui->deviceComboBox->setEnabled(false);
        ui->statusLabel->setText(QStringLiteral("마이크를 찾을 수 없습니다."));
    } else {
        ui->deviceComboBox->setEnabled(!m_audioRecorder->isRecording());
        if (!m_audioRecorder->isRecording()) {
            ui->statusLabel->setText(QStringLiteral("대기 중"));
        }
    }

    setRecordingControls(m_audioRecorder->isRecording());
}

void MainWindow::startRecording()
{
    if (m_aiBackendClient->isRunning()) {
        statusBar()->showMessage(QStringLiteral("AI 작업이 끝난 뒤 새 녹음을 시작하세요."));
        return;
    }

    if (m_audioRecorder->isRecording()) {
        handleRecordingError(QStringLiteral("이미 녹음 중입니다."));
        return;
    }

    if (!ui->deviceComboBox->isEnabled()
        || ui->deviceComboBox->currentIndex() < 0) {
        handleRecordingError(QStringLiteral("사용할 수 있는 마이크가 선택되지 않았습니다."));
        return;
    }

    const MeetingStorage::Result result = m_storage.createMeeting(
        ui->titleEdit->text(), QDateTime::currentDateTime());
    if (!result.succeeded()) {
        handleRecordingError(result.errorMessage);
        return;
    }

    m_currentMeetingDirectory = result.paths.directory;
    m_currentWavPath = result.paths.wav;
    m_recordingStarted = false;

    const QByteArray deviceId = ui->deviceComboBox->currentData().toByteArray();
    if (!m_audioRecorder->startRecording(deviceId, m_currentWavPath)
        && !m_audioRecorder->isRecording()
        && !m_currentMeetingDirectory.isEmpty()) {
        handleRecordingError(QStringLiteral("녹음을 시작할 수 없습니다."));
    }
}

void MainWindow::stopRecording()
{
    if (!m_audioRecorder->isRecording()) {
        return;
    }

    ui->stopRecordingButton->setEnabled(false);
    ui->statusLabel->setText(QStringLiteral("녹음 종료 중..."));
    m_audioRecorder->stopRecording();
}

void MainWindow::handleRecordingStarted()
{
    m_recordingStarted = true;
    m_lastMeetingDirectory.clear();
    m_lastRecordingPath.clear();
    m_elapsedClock.restart();
    m_elapsedTimer->start();
    ui->elapsedTimeLabel->setText(QStringLiteral("00:00:00"));
    ui->statusLabel->setText(QStringLiteral("녹음 중"));
    ui->backendStatusLabel->setText(QStringLiteral("Idle — 대기 중"));
    ui->backendMessageLabel->clear();
    ui->backendProgressBar->setRange(0, 1);
    ui->backendProgressBar->setValue(0);
    setRecordingControls(true);
    statusBar()->showMessage(QStringLiteral("회의 음성을 로컬 WAV 파일로 저장하고 있습니다."));
}

void MainWindow::handleRecordingStopped(const QString &filePath)
{
    updateElapsedTime();
    m_elapsedTimer->stop();
    m_lastRecordingPath = QFileInfo(filePath).absoluteFilePath();
    m_lastMeetingDirectory = QFileInfo(filePath).absolutePath();
    ui->statusLabel->setText(QStringLiteral("녹음 완료"));
    setRecordingControls(false);
    statusBar()->showMessage(
        QStringLiteral("WAV 저장 완료: %1")
            .arg(QDir::toNativeSeparators(filePath)));
    m_currentMeetingDirectory.clear();
    m_currentWavPath.clear();
    m_recordingStarted = false;
}

void MainWindow::handleRecordingError(const QString &message)
{
    if (m_audioRecorder->isRecording()) {
        ui->statusLabel->setText(
            QStringLiteral("녹음 중 - %1").arg(message));
        setRecordingControls(true);
        statusBar()->showMessage(message);
        return;
    }

    m_elapsedTimer->stop();
    if (!m_recordingStarted) {
        discardUnusedMeeting();
        ui->elapsedTimeLabel->setText(QStringLiteral("00:00:00"));
    }
    ui->statusLabel->setText(QStringLiteral("오류: %1").arg(message));
    setRecordingControls(false);
    statusBar()->showMessage(message);
    m_currentMeetingDirectory.clear();
    m_currentWavPath.clear();
    m_recordingStarted = false;
}

void MainWindow::updateElapsedTime()
{
    if (!m_elapsedClock.isValid()) {
        return;
    }

    const qint64 totalSeconds = m_elapsedClock.elapsed() / 1000;
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 seconds = totalSeconds % 60;
    ui->elapsedTimeLabel->setText(
        QStringLiteral("%1:%2:%3")
            .arg(hours, 2, 10, QLatin1Char('0'))
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(seconds, 2, 10, QLatin1Char('0')));
}

void MainWindow::setRecordingControls(bool recording)
{
    const bool backendRunning = m_aiBackendClient->isRunning();
    const bool hasDevice = ui->deviceComboBox->count() > 0
                           && ui->deviceComboBox->itemData(0).isValid();
    ui->titleEdit->setEnabled(!recording && !backendRunning);
    ui->deviceComboBox->setEnabled(!recording && !backendRunning && hasDevice);
    ui->refreshDevicesButton->setEnabled(!recording && !backendRunning);
    ui->startRecordingButton->setEnabled(!recording && !backendRunning && hasDevice);
    ui->stopRecordingButton->setEnabled(recording);
    ui->generateMinutesButton->setEnabled(
        !recording && !backendRunning && !m_lastRecordingPath.isEmpty());
}

void MainWindow::discardUnusedMeeting()
{
    if (m_currentMeetingDirectory.isEmpty()) {
        return;
    }

    if (!m_currentWavPath.isEmpty()) {
        QFile::remove(m_currentWavPath);
    }

    QDir().rmdir(m_currentMeetingDirectory);
}

void MainWindow::startBackendProcessing()
{
    if (m_aiBackendClient->isRunning()) {
        statusBar()->showMessage(QStringLiteral("AI 백엔드 작업이 이미 실행 중입니다."));
        return;
    }
    if (m_lastRecordingPath.isEmpty() || m_lastMeetingDirectory.isEmpty()) {
        handleBackendFailed(QStringLiteral("먼저 회의를 녹음해 주세요."));
        return;
    }

    const QDir meetingDirectory(m_lastMeetingDirectory);
    const QString outputPath = meetingDirectory.filePath(QStringLiteral("meeting.json"));
    const QString transcriptPath = meetingDirectory.filePath(QStringLiteral("transcript.txt"));
    if (!m_aiBackendClient->start(m_lastRecordingPath,
                                  outputPath,
                                  transcriptPath)) {
        setRecordingControls(false);
    }
}

void MainWindow::handleBackendStateChanged(AiBackendClient::State state,
                                           const QString &message)
{
    switch (state) {
    case AiBackendClient::State::Idle:
        ui->backendStatusLabel->setText(QStringLiteral("Idle — 대기 중"));
        ui->backendProgressBar->setRange(0, 1);
        ui->backendProgressBar->setValue(0);
        break;
    case AiBackendClient::State::Transcribing:
        ui->backendStatusLabel->setText(
            QStringLiteral("Transcribing — 음성을 텍스트로 변환 중"));
        ui->backendProgressBar->setRange(0, 0);
        break;
    case AiBackendClient::State::Analyzing:
        ui->backendStatusLabel->setText(
            QStringLiteral("Analyzing — 회의 내용을 분석 중"));
        ui->backendProgressBar->setRange(0, 0);
        break;
    case AiBackendClient::State::Completed:
        ui->backendStatusLabel->setText(QStringLiteral("Completed — 처리 완료"));
        ui->backendProgressBar->setRange(0, 1);
        ui->backendProgressBar->setValue(1);
        break;
    case AiBackendClient::State::Error:
        ui->backendStatusLabel->setText(QStringLiteral("Error — 처리 실패"));
        ui->backendProgressBar->setRange(0, 1);
        ui->backendProgressBar->setValue(0);
        break;
    }
    ui->backendMessageLabel->setText(message);
    setRecordingControls(m_audioRecorder->isRecording());
}

void MainWindow::handleBackendCompleted(const QString &outputPath,
                                        const QString &transcriptPath)
{
    if (outputPath.isEmpty()) {
        statusBar()->showMessage(
            QStringLiteral("빈 Transcript 저장 완료: %1")
                .arg(QDir::toNativeSeparators(transcriptPath)));
    } else {
        statusBar()->showMessage(
            QStringLiteral("AI 회의록 저장 완료: %1")
                .arg(QDir::toNativeSeparators(outputPath)));
    }
    setRecordingControls(false);
}

void MainWindow::handleBackendFailed(const QString &message)
{
    ui->backendStatusLabel->setText(QStringLiteral("Error — 처리 실패"));
    ui->backendMessageLabel->setText(message);
    ui->backendProgressBar->setRange(0, 1);
    ui->backendProgressBar->setValue(0);
    statusBar()->showMessage(message);
    setRecordingControls(m_audioRecorder->isRecording());
}
