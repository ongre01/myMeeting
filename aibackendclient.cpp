#include "aibackendclient.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QStandardPaths>

namespace {
QString firstExistingFile(const QStringList &candidates)
{
    for (const QString &candidate : candidates) {
        const QFileInfo info(candidate);
        if (info.isFile()) {
            return info.absoluteFilePath();
        }
    }
    return {};
}

QStringList ancestorCandidates(const QString &relativePath)
{
    QStringList candidates;
    QDir directory(QCoreApplication::applicationDirPath());
    for (int level = 0; level < 5; ++level) {
        candidates.append(directory.filePath(relativePath));
        if (!directory.cdUp()) {
            break;
        }
    }

    QDir workingDirectory(QDir::currentPath());
    for (int level = 0; level < 3; ++level) {
        candidates.append(workingDirectory.filePath(relativePath));
        if (!workingDirectory.cdUp()) {
            break;
        }
    }
    candidates.removeDuplicates();
    return candidates;
}
} // namespace

AiBackendClient::AiBackendClient(QObject *parent)
    : AiBackendClient(defaultPythonExecutable(), defaultBackendScript(), parent)
{
}

AiBackendClient::AiBackendClient(const QString &pythonExecutable,
                                 const QString &backendScript,
                                 QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
    , m_pythonExecutable(pythonExecutable)
    , m_backendScript(backendScript)
{
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &AiBackendClient::readStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError,
            this, &AiBackendClient::readStandardError);
    connect(m_process, &QProcess::errorOccurred,
            this, &AiBackendClient::handleProcessError);
    connect(m_process,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            &AiBackendClient::handleFinished);
}

AiBackendClient::~AiBackendClient()
{
    if (m_process->state() == QProcess::NotRunning) {
        return;
    }

    m_process->terminate();
    if (!m_process->waitForFinished(500)) {
        m_process->kill();
        m_process->waitForFinished(500);
    }
}

bool AiBackendClient::start(const QString &inputPath,
                            const QString &outputPath,
                            const QString &transcriptPath,
                            const QString &configPath)
{
    if (isRunning()) {
        return false;
    }

    resetRequest(outputPath, transcriptPath);

    const QFileInfo inputInfo(inputPath);
    if (!inputInfo.isFile()) {
        finishWithError(QStringLiteral("분석할 WAV 파일을 찾을 수 없습니다: %1")
                            .arg(QDir::toNativeSeparators(inputInfo.absoluteFilePath())));
        return false;
    }

    QString scriptPath;
    if (!validateRuntime(&scriptPath)) {
        return false;
    }

    QStringList arguments{
        scriptPath,
        QStringLiteral("--input"),
        inputInfo.absoluteFilePath(),
        QStringLiteral("--output"),
        m_outputPath,
        QStringLiteral("--transcript-output"),
        m_transcriptPath,
        QStringLiteral("--progress")
    };
    if (!configPath.trimmed().isEmpty()) {
        arguments.append({QStringLiteral("--config"),
                          QFileInfo(configPath).absoluteFilePath()});
    }

    startProcess(arguments,
                 State::Transcribing,
                 QStringLiteral("음성을 텍스트로 변환 중..."));
    return true;
}

bool AiBackendClient::analyzeTranscript(const QString &transcriptPath,
                                        const QString &outputPath,
                                        const QString &configPath)
{
    if (isRunning()) {
        return false;
    }

    resetRequest(outputPath, transcriptPath);

    QFile transcriptFile(m_transcriptPath);
    if (!transcriptFile.open(QIODevice::ReadOnly)) {
        finishWithError(QStringLiteral("분석할 Transcript 파일을 읽을 수 없습니다: %1")
                            .arg(QDir::toNativeSeparators(m_transcriptPath)));
        return false;
    }
    if (transcriptFile.readAll().trimmed().isEmpty()) {
        finishWithError(QStringLiteral("빈 Transcript는 회의록으로 분석할 수 없습니다."));
        return false;
    }

    QString scriptPath;
    if (!validateRuntime(&scriptPath)) {
        return false;
    }

    QStringList arguments{
        scriptPath,
        QStringLiteral("--transcript-input"),
        m_transcriptPath,
        QStringLiteral("--output"),
        m_outputPath,
        QStringLiteral("--progress")
    };
    if (!configPath.trimmed().isEmpty()) {
        arguments.append({QStringLiteral("--config"),
                          QFileInfo(configPath).absoluteFilePath()});
    }

    startProcess(arguments,
                 State::Analyzing,
                 QStringLiteral("AI가 수정된 Transcript를 분석하고 있습니다..."));
    return true;
}

bool AiBackendClient::isRunning() const
{
    return m_process->state() != QProcess::NotRunning;
}

AiBackendClient::State AiBackendClient::state() const
{
    return m_state;
}

QString AiBackendClient::errorMessage() const
{
    return m_errorMessage;
}

QString AiBackendClient::defaultPythonExecutable()
{
    const QString configured = qEnvironmentVariable("MYMEETING_PYTHON_EXECUTABLE");
    if (!configured.trimmed().isEmpty()) {
        return configured;
    }

    QStringList candidates = ancestorCandidates(QStringLiteral(".venv/Scripts/python.exe"));
    const QString virtualEnvironmentPython = firstExistingFile(candidates);
    if (!virtualEnvironmentPython.isEmpty()) {
        return virtualEnvironmentPython;
    }

    QString executable = QStandardPaths::findExecutable(QStringLiteral("python"));
    if (executable.isEmpty()) {
        executable = QStandardPaths::findExecutable(QStringLiteral("python3"));
    }
    return executable;
}

QString AiBackendClient::defaultBackendScript()
{
    const QString configured = qEnvironmentVariable("MYMEETING_BACKEND_SCRIPT");
    if (!configured.trimmed().isEmpty()) {
        return configured;
    }
    return firstExistingFile(ancestorCandidates(QStringLiteral("backend/main.py")));
}

void AiBackendClient::readStandardOutput()
{
    m_standardOutputBuffer.append(m_process->readAllStandardOutput());
    processOutputBuffer(false);
}

void AiBackendClient::readStandardError()
{
    m_standardError.append(m_process->readAllStandardError());
}

void AiBackendClient::processOutputBuffer(bool flushRemainder)
{
    qsizetype newline = -1;
    while ((newline = m_standardOutputBuffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_standardOutputBuffer.left(newline).trimmed();
        m_standardOutputBuffer.remove(0, newline + 1);
        if (!line.isEmpty()) {
            processOutputLine(line);
        }
    }

    if (flushRemainder && !m_standardOutputBuffer.trimmed().isEmpty()) {
        processOutputLine(m_standardOutputBuffer.trimmed());
        m_standardOutputBuffer.clear();
    }
}

void AiBackendClient::processOutputLine(const QByteArray &line)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (m_protocolError.isEmpty()) {
            m_protocolError = QStringLiteral("백엔드가 올바른 JSON 상태를 반환하지 않았습니다.");
        }
        return;
    }

    const QJsonObject result = document.object();
    const QString status = result.value(QStringLiteral("status")).toString();
    if (status == QStringLiteral("transcribing")) {
        setState(State::Transcribing,
                 QStringLiteral("음성을 텍스트로 변환 중..."));
    } else if (status == QStringLiteral("analyzing")) {
        setState(State::Analyzing,
                 QStringLiteral("AI가 회의 내용을 분석하고 있습니다..."));
    } else if (status == QStringLiteral("completed")
               || status == QStringLiteral("transcribed")) {
        m_finalResult = result;
    } else if (m_protocolError.isEmpty()) {
        m_protocolError = QStringLiteral("백엔드가 알 수 없는 상태를 반환했습니다.");
    }
}

void AiBackendClient::handleProcessError(QProcess::ProcessError error)
{
    if (m_terminalSignalSent) {
        return;
    }

    if (error == QProcess::FailedToStart) {
        finishWithError(QStringLiteral("백엔드 실행 파일을 시작할 수 없습니다: %1")
                            .arg(m_process->errorString()));
    } else if (error == QProcess::Crashed) {
        finishWithError(QStringLiteral("AI 백엔드가 비정상 종료되었습니다."));
    } else if (error == QProcess::ReadError || error == QProcess::WriteError) {
        finishWithError(QStringLiteral("AI 백엔드와 통신할 수 없습니다: %1")
                            .arg(m_process->errorString()));
        m_process->kill();
    }
}

void AiBackendClient::handleFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_standardOutputBuffer.append(m_process->readAllStandardOutput());
    m_standardError.append(m_process->readAllStandardError());
    processOutputBuffer(true);

    if (m_terminalSignalSent) {
        return;
    }
    if (exitStatus != QProcess::NormalExit) {
        finishWithError(QStringLiteral("AI 백엔드가 비정상 종료되었습니다."));
        return;
    }
    if (exitCode != 0) {
        finishWithError(processFailureMessage(exitCode));
        return;
    }
    if (!m_protocolError.isEmpty()) {
        finishWithError(m_protocolError);
        return;
    }
    if (m_finalResult.isEmpty()) {
        finishWithError(QStringLiteral("백엔드 완료 결과를 받지 못했습니다."));
        return;
    }

    QString validationError;
    if (!validateTranscriptOutput(&validationError)) {
        finishWithError(validationError);
        return;
    }

    const QString status = m_finalResult.value(QStringLiteral("status")).toString();
    if (status == QStringLiteral("completed")) {
        if (!validateMeetingOutput(&validationError)) {
            finishWithError(validationError);
            return;
        }
        m_terminalSignalSent = true;
        setState(State::Completed, QStringLiteral("AI 회의록 생성 완료"));
        emit completed(m_outputPath, m_transcriptPath);
        return;
    }

    m_terminalSignalSent = true;
    setState(State::Completed,
             QStringLiteral("음성이 감지되지 않아 빈 Transcript를 저장했습니다."));
    emit completed(QString(), m_transcriptPath);
}

void AiBackendClient::setState(State state, const QString &message)
{
    if (m_state == state && m_stateMessage == message) {
        return;
    }
    m_state = state;
    m_stateMessage = message;
    emit stateChanged(state, message);
}

void AiBackendClient::finishWithError(const QString &message)
{
    if (m_terminalSignalSent) {
        return;
    }
    m_terminalSignalSent = true;
    m_errorMessage = message.trimmed().isEmpty()
                         ? QStringLiteral("AI 백엔드 작업에 실패했습니다.")
                         : message.trimmed();
    setState(State::Error, m_errorMessage);
    emit failed(m_errorMessage);
}

void AiBackendClient::resetRequest(const QString &outputPath,
                                   const QString &transcriptPath)
{
    m_terminalSignalSent = false;
    m_errorMessage.clear();
    m_protocolError.clear();
    m_standardOutputBuffer.clear();
    m_standardError.clear();
    m_finalResult = {};
    m_outputPath = QFileInfo(outputPath).absoluteFilePath();
    m_transcriptPath = QFileInfo(transcriptPath).absoluteFilePath();
}

bool AiBackendClient::validateRuntime(QString *scriptPath)
{
    if (m_pythonExecutable.trimmed().isEmpty()) {
        finishWithError(QStringLiteral("Python 3.11 이상 실행 파일을 찾을 수 없습니다."));
        return false;
    }

    const QFileInfo executableInfo(m_pythonExecutable);
    if (executableInfo.isAbsolute() && !executableInfo.isFile()) {
        finishWithError(QStringLiteral("백엔드 실행 파일을 찾을 수 없습니다: %1")
                            .arg(QDir::toNativeSeparators(m_pythonExecutable)));
        return false;
    }

    const QFileInfo scriptInfo(m_backendScript);
    if (!scriptInfo.isFile()) {
        finishWithError(QStringLiteral("Python 백엔드 스크립트를 찾을 수 없습니다: %1")
                            .arg(QDir::toNativeSeparators(scriptInfo.absoluteFilePath())));
        return false;
    }

    *scriptPath = scriptInfo.absoluteFilePath();
    return true;
}

void AiBackendClient::startProcess(const QStringList &arguments,
                                   State initialState,
                                   const QString &message)
{
    m_process->setProgram(m_pythonExecutable);
    m_process->setArguments(arguments);
    m_process->start();
    setState(initialState, message);
}

QString AiBackendClient::processFailureMessage(int exitCode) const
{
    const QString error = QString::fromUtf8(m_standardError).trimmed();
    if (error.startsWith(QStringLiteral("오류:")) && error.size() <= 1000) {
        return error.mid(QStringLiteral("오류:").size()).trimmed();
    }
    return QStringLiteral("AI 백엔드 작업이 실패했습니다. (종료 코드 %1)")
        .arg(exitCode);
}

bool AiBackendClient::validateTranscriptOutput(QString *errorMessage) const
{
    const QFileInfo info(m_transcriptPath);
    if (!info.isFile() || !info.isReadable()) {
        *errorMessage = QStringLiteral("백엔드가 Transcript 출력 파일을 만들지 못했습니다: %1")
                            .arg(QDir::toNativeSeparators(m_transcriptPath));
        return false;
    }
    return true;
}

bool AiBackendClient::validateMeetingOutput(QString *errorMessage) const
{
    QFile file(m_outputPath);
    if (!file.open(QIODevice::ReadOnly)) {
        *errorMessage = QStringLiteral("백엔드 회의록 출력 파일을 읽을 수 없습니다: %1")
                            .arg(QDir::toNativeSeparators(m_outputPath));
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        *errorMessage = QStringLiteral("백엔드 회의록 출력 파일이 올바른 JSON 객체가 아닙니다.");
        return false;
    }
    return true;
}
