#ifndef AIBACKENDCLIENT_H
#define AIBACKENDCLIENT_H

#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QString>

class AiBackendClient : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Idle,
        Transcribing,
        Analyzing,
        Completed,
        Error
    };
    Q_ENUM(State)

    explicit AiBackendClient(QObject *parent = nullptr);
    AiBackendClient(const QString &pythonExecutable,
                    const QString &backendScript,
                    QObject *parent = nullptr);
    ~AiBackendClient() override;

    bool start(const QString &inputPath,
               const QString &outputPath,
               const QString &transcriptPath,
               const QString &configPath = QString());
    bool analyzeTranscript(const QString &transcriptPath,
                           const QString &outputPath,
                           const QString &configPath = QString());
    bool isRunning() const;
    State state() const;
    QString errorMessage() const;

signals:
    void stateChanged(AiBackendClient::State state, const QString &message);
    void completed(const QString &outputPath, const QString &transcriptPath);
    void failed(const QString &message);

private:
    static QString defaultPythonExecutable();
    static QString defaultBackendScript();

    void readStandardOutput();
    void readStandardError();
    void processOutputBuffer(bool flushRemainder);
    void processOutputLine(const QByteArray &line);
    void handleProcessError(QProcess::ProcessError error);
    void handleFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void setState(State state, const QString &message);
    void finishWithError(const QString &message);
    void resetRequest(const QString &outputPath,
                      const QString &transcriptPath);
    bool validateRuntime(QString *scriptPath);
    void startProcess(const QStringList &arguments,
                      State initialState,
                      const QString &message);
    QString processFailureMessage(int exitCode) const;
    bool validateTranscriptOutput(QString *errorMessage) const;
    bool validateMeetingOutput(QString *errorMessage) const;

    QProcess *m_process;
    QString m_pythonExecutable;
    QString m_backendScript;
    State m_state = State::Idle;
    QString m_stateMessage;
    QString m_errorMessage;
    QString m_outputPath;
    QString m_transcriptPath;
    QByteArray m_standardOutputBuffer;
    QByteArray m_standardError;
    QJsonObject m_finalResult;
    QString m_protocolError;
    bool m_terminalSignalSent = false;
};

#endif // AIBACKENDCLIENT_H
