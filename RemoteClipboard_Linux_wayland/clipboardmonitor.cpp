#include "clipboardmonitor.h"

#include <QDebug>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QUrl>

ClipboardMonitor::ClipboardMonitor(QObject *parent)
    : QObject(parent)
    , checkTimer(new QTimer(this))
    , isMonitoring(false)
{
    checkTimer->setInterval(500);
    connect(checkTimer, &QTimer::timeout, this, &ClipboardMonitor::checkClipboard);
}

ClipboardMonitor::~ClipboardMonitor()
{
    stopMonitoring();
}

void ClipboardMonitor::startMonitoring()
{
    if (!isMonitoring) {
        checkClipboard();
        checkTimer->start();
        isMonitoring = true;
    }
}

void ClipboardMonitor::stopMonitoring()
{
    if (isMonitoring) {
        checkTimer->stop();
        isMonitoring = false;
    }
}

void ClipboardMonitor::setClipboardText(const QString &content)
{
    if (content.isNull()) {
        return;
    }

    QProcess process;
    process.start(QStringLiteral("wl-copy"), {});
    process.write(content.toUtf8());
    process.closeWriteChannel();
    process.waitForFinished();
    lastSignature = QStringLiteral("text:") + content;
}

void ClipboardMonitor::checkClipboard()
{
    const QStringList filePaths = readClipboardFiles();
    if (!filePaths.isEmpty()) {
        const QString signature = buildSignatureForFiles(filePaths);
        if (signature != lastSignature) {
            lastSignature = signature;
            emit filesChanged(filePaths);
        }
        return;
    }

    const QString text = readClipboardText();
    const QString signature = QStringLiteral("text:") + text;
    if (signature != lastSignature) {
        lastSignature = signature;
        emit textChanged(text);
    }
}

QString ClipboardMonitor::runCommand(const QString& program, const QStringList& arguments) const
{
    QProcess process;
    process.start(program, arguments);
    process.waitForFinished();
    return QString::fromUtf8(process.readAllStandardOutput());
}

QStringList ClipboardMonitor::readClipboardFiles() const
{
    const QString mimeTypes = runCommand(QStringLiteral("wl-paste"), {QStringLiteral("--list-types")});
    if (!mimeTypes.contains(QStringLiteral("text/uri-list"))) {
        return {};
    }

    const QString uriPayload = runCommand(QStringLiteral("wl-paste"),
        {QStringLiteral("--type"), QStringLiteral("text/uri-list"), QStringLiteral("--no-newline")});

    QStringList files;
    const QStringList lines = uriPayload.split(QRegularExpression(QStringLiteral("[\r\n]+")), Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        const QUrl url(line.trimmed());
        if (url.isLocalFile()) {
            const QString localPath = url.toLocalFile();
            if (QFileInfo::exists(localPath)) {
                files.append(localPath);
            }
        }
    }
    return files;
}

QString ClipboardMonitor::readClipboardText() const
{
    return runCommand(QStringLiteral("wl-paste"), {});
}

QString ClipboardMonitor::buildSignatureForFiles(const QStringList& filePaths) const
{
    return QStringLiteral("files:") + filePaths.join(QStringLiteral("|"));
}
