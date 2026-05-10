#include "videorendercontroller.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

VideoRenderController::VideoRenderController(QObject *parent)
    : QObject(parent)
{
    connect(&m_probeProcess, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this, &VideoRenderController::onProbeFinished);
    connect(&m_extractProcess, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this, &VideoRenderController::onExtractFinished);
    connect(&m_encodeProcess, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this, &VideoRenderController::onEncodeFinished);
}

bool VideoRenderController::renderMode() const
{
    return m_renderMode;
}

bool VideoRenderController::running() const
{
    return m_running;
}

bool VideoRenderController::finished() const
{
    return m_finished;
}

bool VideoRenderController::failed() const
{
    return m_failed;
}

QString VideoRenderController::inputPath() const
{
    return m_inputPath;
}

QString VideoRenderController::outputPath() const
{
    return m_outputPath;
}

QString VideoRenderController::currentFrameSource() const
{
    return m_currentFrameSource;
}

QString VideoRenderController::currentFrameOutput() const
{
    return m_currentFrameOutput;
}

QString VideoRenderController::statusText() const
{
    return m_statusText;
}

QString VideoRenderController::logText() const
{
    return m_logText;
}

QString VideoRenderController::warningsText() const
{
    return m_warningsText;
}

QString VideoRenderController::renderTitle() const
{
    return m_renderTitle;
}

int VideoRenderController::frameWidth() const
{
    return m_frameWidth;
}

int VideoRenderController::frameHeight() const
{
    return m_frameHeight;
}

int VideoRenderController::totalFrames() const
{
    return m_totalFrames;
}

int VideoRenderController::renderedFrames() const
{
    return m_renderedFrames;
}

int VideoRenderController::currentFrameNumber() const
{
    return m_currentFrameIndex >= 0 ? m_currentFrameIndex + 1 : 0;
}

double VideoRenderController::animationTime() const
{
    return m_animationTime;
}

void VideoRenderController::configureRenderJob(const QString &inputPath,
                                               const QString &outputPath,
                                               const QStringList &ignoredArguments)
{
    m_renderMode = !inputPath.isEmpty();
    if (!m_renderMode) {
        return;
    }

    m_inputPath = QFileInfo(inputPath).absoluteFilePath();
    m_outputPath = outputPath.isEmpty()
        ? deriveDefaultOutputPath(m_inputPath)
        : QFileInfo(outputPath).absoluteFilePath();
    m_renderTitle = QStringLiteral("Video Renderer");

    if (!ignoredArguments.isEmpty()) {
        QStringList warningLines;
        warningLines << QStringLiteral("Ignored in --render-video mode:");
        for (const QString &argument : ignoredArguments) {
            warningLines << QStringLiteral("  %1").arg(argument);
        }
        m_warningsText = warningLines.join(QLatin1Char('\n'));
    }

    emit inputPathChanged();
    emit outputPathChanged();
    emit warningsTextChanged();
    emit renderTitleChanged();
}

void VideoRenderController::start()
{
    if (!m_renderMode || m_running || m_finished) {
        return;
    }

    if (!QFileInfo::exists(m_inputPath)) {
        fail(QStringLiteral("Input video does not exist: %1").arg(m_inputPath));
        return;
    }

    if (ffmpegExecutable().isEmpty()) {
        fail(QStringLiteral("Could not find ffmpeg in PATH."));
        return;
    }

    if (ffprobeExecutable().isEmpty()) {
        fail(QStringLiteral("Could not find ffprobe in PATH."));
        return;
    }

    const QFileInfo outputInfo(m_outputPath);
    if (!outputInfo.dir().mkpath(QStringLiteral("."))) {
        fail(QStringLiteral("Could not create output directory: %1").arg(outputInfo.dir().absolutePath()));
        return;
    }

    if (!ensureTempWorkspace()) {
        fail(QStringLiteral("Could not create a temporary rendering workspace."));
        return;
    }

    m_running = true;
    emit runningChanged();

    appendLogLine(QStringLiteral("Input:  %1").arg(m_inputPath));
    appendLogLine(QStringLiteral("Output: %1").arg(m_outputPath));
    if (!m_warningsText.isEmpty()) {
        appendLogLine(m_warningsText);
    }

    startProbe();
}

void VideoRenderController::completeCurrentFrame(bool saved)
{
    if (m_stage != Stage::Rendering) {
        return;
    }

    if (!saved) {
        fail(QStringLiteral("Failed to capture rendered frame %1.").arg(currentFrameNumber()));
        return;
    }

    m_renderedFrames += 1;
    emitProgressChanged();

    m_currentFrameIndex += 1;
    QTimer::singleShot(0, this, &VideoRenderController::queueNextFrame);
}

void VideoRenderController::onProbeFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (m_stage != Stage::Probing) {
        return;
    }

    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        fail(QStringLiteral("ffprobe failed: %1").arg(QString::fromUtf8(m_probeProcess.readAllStandardError()).trimmed()));
        return;
    }

    if (!parseProbeOutput(m_probeProcess.readAllStandardOutput())) {
        fail(QStringLiteral("Could not parse ffprobe metadata."));
        return;
    }

    emit frameSizeChanged();
    emitProgressChanged();

    appendLogLine(QStringLiteral("Detected video size %1x%2 at %3 fps")
                  .arg(m_frameWidth)
                  .arg(m_frameHeight)
                  .arg(QString::number(m_fps, 'f', 3)));

    startExtraction();
}

void VideoRenderController::onExtractFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (m_stage != Stage::Extracting) {
        return;
    }

    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        fail(QStringLiteral("ffmpeg frame extraction failed: %1")
             .arg(QString::fromUtf8(m_extractProcess.readAllStandardError()).trimmed()));
        return;
    }

    QDir inputDir(m_inputFramesDir);
    m_inputFrameFiles = inputDir.entryList(QStringList() << QStringLiteral("*.png"), QDir::Files, QDir::Name);
    if (m_inputFrameFiles.isEmpty()) {
        fail(QStringLiteral("ffmpeg did not extract any frames."));
        return;
    }

    m_totalFrames = m_inputFrameFiles.size();
    emitProgressChanged();

    appendLogLine(QStringLiteral("Extracted %1 frame(s)").arg(m_totalFrames));
    startFrameRendering();
}

void VideoRenderController::onEncodeFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (m_stage != Stage::Encoding) {
        return;
    }

    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        fail(QStringLiteral("ffmpeg encoding failed: %1")
             .arg(QString::fromUtf8(m_encodeProcess.readAllStandardError()).trimmed()));
        return;
    }

    m_running = false;
    m_finished = true;
    m_failed = false;
    m_currentFrameSource.clear();
    m_currentFrameOutput.clear();

    setStage(Stage::Completed, QStringLiteral("Rendering complete."));
    appendLogLine(QStringLiteral("Wrote rendered video to %1").arg(m_outputPath));

    emit runningChanged();
    emit finishedChanged();
    emit failedChanged();
    emit currentFrameSourceChanged();
    emit currentFrameOutputChanged();
}

void VideoRenderController::setStage(Stage stage, const QString &status)
{
    m_stage = stage;
    if (m_statusText == status) {
        return;
    }

    m_statusText = status;
    emit statusTextChanged();
}

void VideoRenderController::fail(const QString &message)
{
    m_running = false;
    m_finished = true;
    m_failed = true;
    m_currentFrameSource.clear();
    m_currentFrameOutput.clear();

    setStage(Stage::Failed, message);
    appendLogLine(message);

    emit runningChanged();
    emit finishedChanged();
    emit failedChanged();
    emit currentFrameSourceChanged();
    emit currentFrameOutputChanged();
}

void VideoRenderController::appendLogLine(const QString &line)
{
    if (line.isEmpty()) {
        return;
    }

    if (!m_logText.isEmpty()) {
        m_logText.append(QLatin1Char('\n'));
    }
    m_logText.append(line);
    emit logTextChanged();
}

void VideoRenderController::emitProgressChanged()
{
    emit progressChanged();
}

void VideoRenderController::startProbe()
{
    setStage(Stage::Probing, QStringLiteral("Probing input video..."));
    appendLogLine(QStringLiteral("Running ffprobe metadata scan"));

    const QStringList arguments = {
        QStringLiteral("-v"), QStringLiteral("error"),
        QStringLiteral("-select_streams"), QStringLiteral("v:0"),
        QStringLiteral("-show_entries"), QStringLiteral("stream=width,height,avg_frame_rate,r_frame_rate,nb_frames"),
        QStringLiteral("-of"), QStringLiteral("json"),
        m_inputPath,
    };
    m_probeProcess.start(ffprobeExecutable(), arguments);
}

void VideoRenderController::startExtraction()
{
    setStage(Stage::Extracting, QStringLiteral("Extracting frames with ffmpeg..."));
    appendLogLine(QStringLiteral("Extracting source frames to %1").arg(m_inputFramesDir));

    const QString outputPattern = QDir(m_inputFramesDir).filePath(QStringLiteral("%08d.png"));
    const QStringList arguments = {
        QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-y"),
        QStringLiteral("-i"), m_inputPath,
        QStringLiteral("-vsync"), QStringLiteral("0"),
        QStringLiteral("-start_number"), QStringLiteral("0"),
        outputPattern,
    };
    m_extractProcess.start(ffmpegExecutable(), arguments);
}

void VideoRenderController::startFrameRendering()
{
    m_renderedFrames = 0;
    m_currentFrameIndex = 0;
    emitProgressChanged();

    setStage(Stage::Rendering, QStringLiteral("Rendering frames through CRT shader pipeline..."));
    queueNextFrame();
}

void VideoRenderController::queueNextFrame()
{
    if (m_stage != Stage::Rendering) {
        return;
    }

    if (m_currentFrameIndex < 0 || m_currentFrameIndex >= m_inputFrameFiles.size()) {
        m_currentFrameSource.clear();
        m_currentFrameOutput.clear();
        emit currentFrameSourceChanged();
        emit currentFrameOutputChanged();
        startEncoding();
        return;
    }

    const QString inputFileName = m_inputFrameFiles.at(m_currentFrameIndex);
    const QString inputFramePath = QDir(m_inputFramesDir).filePath(inputFileName);
    const QString outputFramePath = QDir(m_outputFramesDir).filePath(inputFileName);

    m_animationTime = static_cast<double>(m_currentFrameIndex) / m_fps;
    emit animationTimeChanged();

    m_currentFrameSource = QUrl::fromLocalFile(inputFramePath).toString();
    m_currentFrameOutput = outputFramePath;

    setStage(Stage::Rendering,
             QStringLiteral("Rendering frame %1 of %2...")
             .arg(currentFrameNumber())
             .arg(m_totalFrames));

    emit currentFrameSourceChanged();
    emit currentFrameOutputChanged();
    emitProgressChanged();
}

void VideoRenderController::startEncoding()
{
    setStage(Stage::Encoding, QStringLiteral("Encoding output video with ffmpeg..."));
    appendLogLine(QStringLiteral("Encoding rendered frames to %1").arg(m_outputPath));

    const QString inputPattern = QDir(m_outputFramesDir).filePath(QStringLiteral("%08d.png"));
    const QString fps = QString::number(m_fps, 'f', 6);
    const QStringList arguments = {
        QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-y"),
        QStringLiteral("-framerate"), fps,
        QStringLiteral("-start_number"), QStringLiteral("0"),
        QStringLiteral("-i"), inputPattern,
        QStringLiteral("-i"), m_inputPath,
        QStringLiteral("-map"), QStringLiteral("0:v:0"),
        QStringLiteral("-map"), QStringLiteral("1:a?"),
        QStringLiteral("-c:v"), QStringLiteral("libx264"),
        QStringLiteral("-preset"), QStringLiteral("slow"),
        QStringLiteral("-crf"), QStringLiteral("10"),
        QStringLiteral("-pix_fmt"), QStringLiteral("yuv420p"),
        QStringLiteral("-c:a"), QStringLiteral("aac"),
        QStringLiteral("-b:a"), QStringLiteral("192k"),
        QStringLiteral("-movflags"), QStringLiteral("+faststart"),
        QStringLiteral("-shortest"),
        m_outputPath,
    };
    m_encodeProcess.start(ffmpegExecutable(), arguments);
}

QString VideoRenderController::deriveDefaultOutputPath(const QString &inputPath) const
{
    const QFileInfo inputInfo(inputPath);
    const QString baseName = inputInfo.completeBaseName().isEmpty()
        ? QStringLiteral("rendered-video")
        : inputInfo.completeBaseName();
    return inputInfo.dir().filePath(baseName + QStringLiteral("-crt.mp4"));
}

QString VideoRenderController::ffmpegExecutable() const
{
    return QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
}

QString VideoRenderController::ffprobeExecutable() const
{
    return QStandardPaths::findExecutable(QStringLiteral("ffprobe"));
}

bool VideoRenderController::ensureTempWorkspace()
{
    const QString workRoot = renderWorkRoot();
    if (!workRoot.isEmpty()) {
        const QString renderWorkspacePath = QDir(workRoot).filePath(QStringLiteral("render-work"));
        m_inputFramesDir = QDir(renderWorkspacePath).filePath(QStringLiteral("input_frames"));
        m_outputFramesDir = QDir(renderWorkspacePath).filePath(QStringLiteral("output_frames"));
    } else {
        if (!m_tempWorkspace.isValid()) {
            return false;
        }
        m_inputFramesDir = QDir(m_tempWorkspace.path()).filePath(QStringLiteral("input_frames"));
        m_outputFramesDir = QDir(m_tempWorkspace.path()).filePath(QStringLiteral("output_frames"));
    }

    if (!resetDirectory(m_inputFramesDir)
        || !resetDirectory(m_outputFramesDir)) {
        return false;
    }

    return true;
}

bool VideoRenderController::parseProbeOutput(const QByteArray &data)
{
    const QJsonDocument document = QJsonDocument::fromJson(data);
    if (!document.isObject()) {
        return false;
    }

    const QJsonArray streams = document.object().value(QStringLiteral("streams")).toArray();
    if (streams.isEmpty() || !streams.first().isObject()) {
        return false;
    }

    const QJsonObject stream = streams.first().toObject();
    m_frameWidth = stream.value(QStringLiteral("width")).toInt();
    m_frameHeight = stream.value(QStringLiteral("height")).toInt();
    if (m_frameWidth <= 0 || m_frameHeight <= 0) {
        return false;
    }

    const QString avgFrameRate = stream.value(QStringLiteral("avg_frame_rate")).toString();
    const QString realFrameRate = stream.value(QStringLiteral("r_frame_rate")).toString();
    m_fps = fpsFromRateString(avgFrameRate);
    if (m_fps <= 0.0) {
        m_fps = fpsFromRateString(realFrameRate);
    }
    if (m_fps <= 0.0) {
        m_fps = 30.0;
    }

    const QString nbFrames = stream.value(QStringLiteral("nb_frames")).toString();
    bool ok = false;
    const int totalFrames = nbFrames.toInt(&ok);
    if (ok && totalFrames > 0) {
        m_totalFrames = totalFrames;
    }

    return true;
}

double VideoRenderController::fpsFromRateString(const QString &rate) const
{
    const QString trimmed = rate.trimmed();
    if (trimmed.isEmpty() || trimmed == QStringLiteral("0/0")) {
        return 0.0;
    }

    const QStringList parts = trimmed.split(QLatin1Char('/'));
    if (parts.size() == 2) {
        bool numeratorOk = false;
        bool denominatorOk = false;
        const double numerator = parts.at(0).toDouble(&numeratorOk);
        const double denominator = parts.at(1).toDouble(&denominatorOk);
        if (numeratorOk && denominatorOk && denominator > 0.0) {
            return numerator / denominator;
        }
    }

    bool ok = false;
    const double value = trimmed.toDouble(&ok);
    return ok ? value : 0.0;
}

QString VideoRenderController::renderWorkRoot() const
{
    const QByteArray fromEnvironment = qgetenv("CRT_RENDER_WORK_ROOT");
    if (!fromEnvironment.isEmpty()) {
        return QString::fromUtf8(fromEnvironment);
    }

    return QStringLiteral("/tmp/cool-retro-term-working-dir");
}

bool VideoRenderController::resetDirectory(const QString &path)
{
    const QFileInfo info(path);
    if (info.exists()) {
        QDir existingDir(path);
        if (!existingDir.removeRecursively()) {
            return false;
        }
    }

    QDir dir;
    return dir.mkpath(path);
}
