#ifndef VIDEORENDERCONTROLLER_H
#define VIDEORENDERCONTROLLER_H

#include <QObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QStringList>

class VideoRenderController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool renderMode READ renderMode CONSTANT)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(bool finished READ finished NOTIFY finishedChanged)
    Q_PROPERTY(bool failed READ failed NOTIFY failedChanged)
    Q_PROPERTY(QString inputPath READ inputPath NOTIFY inputPathChanged)
    Q_PROPERTY(QString outputPath READ outputPath NOTIFY outputPathChanged)
    Q_PROPERTY(QString currentFrameSource READ currentFrameSource NOTIFY currentFrameSourceChanged)
    Q_PROPERTY(QString currentFrameOutput READ currentFrameOutput NOTIFY currentFrameOutputChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString logText READ logText NOTIFY logTextChanged)
    Q_PROPERTY(QString warningsText READ warningsText NOTIFY warningsTextChanged)
    Q_PROPERTY(QString renderTitle READ renderTitle NOTIFY renderTitleChanged)
    Q_PROPERTY(int frameWidth READ frameWidth NOTIFY frameSizeChanged)
    Q_PROPERTY(int frameHeight READ frameHeight NOTIFY frameSizeChanged)
    Q_PROPERTY(int totalFrames READ totalFrames NOTIFY progressChanged)
    Q_PROPERTY(int renderedFrames READ renderedFrames NOTIFY progressChanged)
    Q_PROPERTY(int currentFrameNumber READ currentFrameNumber NOTIFY progressChanged)
    Q_PROPERTY(double animationTime READ animationTime NOTIFY animationTimeChanged)

public:
    explicit VideoRenderController(QObject *parent = nullptr);

    bool renderMode() const;
    bool running() const;
    bool finished() const;
    bool failed() const;

    QString inputPath() const;
    QString outputPath() const;
    QString currentFrameSource() const;
    QString currentFrameOutput() const;
    QString statusText() const;
    QString logText() const;
    QString warningsText() const;
    QString renderTitle() const;
    int frameWidth() const;
    int frameHeight() const;
    int totalFrames() const;
    int renderedFrames() const;
    int currentFrameNumber() const;
    double animationTime() const;

    void configureRenderJob(const QString &inputPath,
                            const QString &outputPath,
                            const QStringList &ignoredArguments);

    Q_INVOKABLE void start();
    Q_INVOKABLE void completeCurrentFrame(bool saved);

signals:
    void runningChanged();
    void finishedChanged();
    void failedChanged();
    void inputPathChanged();
    void outputPathChanged();
    void currentFrameSourceChanged();
    void currentFrameOutputChanged();
    void statusTextChanged();
    void logTextChanged();
    void warningsTextChanged();
    void renderTitleChanged();
    void frameSizeChanged();
    void progressChanged();
    void animationTimeChanged();

private slots:
    void onProbeFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onExtractFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onEncodeFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    enum class Stage {
        Idle,
        Probing,
        Extracting,
        Rendering,
        Encoding,
        Completed,
        Failed,
    };

    void setStage(Stage stage, const QString &status);
    void fail(const QString &message);
    void appendLogLine(const QString &line);
    void emitProgressChanged();
    void startProbe();
    void startExtraction();
    void startFrameRendering();
    void queueNextFrame();
    void startEncoding();
    QString deriveDefaultOutputPath(const QString &inputPath) const;
    QString ffmpegExecutable() const;
    QString ffprobeExecutable() const;
    bool ensureTempWorkspace();
    bool parseProbeOutput(const QByteArray &data);
    double fpsFromRateString(const QString &rate) const;
    QString renderWorkRoot() const;
    bool resetDirectory(const QString &path);

    bool m_renderMode = false;
    bool m_running = false;
    bool m_finished = false;
    bool m_failed = false;

    QString m_inputPath;
    QString m_outputPath;
    QString m_currentFrameSource;
    QString m_currentFrameOutput;
    QString m_statusText;
    QString m_logText;
    QString m_warningsText;
    QString m_renderTitle;

    int m_frameWidth = 0;
    int m_frameHeight = 0;
    int m_totalFrames = 0;
    int m_renderedFrames = 0;
    int m_currentFrameIndex = -1;

    double m_fps = 30.0;
    double m_animationTime = 0.0;

    Stage m_stage = Stage::Idle;
    QTemporaryDir m_tempWorkspace;
    QString m_inputFramesDir;
    QString m_outputFramesDir;
    QStringList m_inputFrameFiles;

    QProcess m_probeProcess;
    QProcess m_extractProcess;
    QProcess m_encodeProcess;
};

#endif // VIDEORENDERCONTROLLER_H
