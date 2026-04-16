#include "clipboardmonitor.h"

#include <QApplication>
#include <QMimeData>
#include <QUrl>

ClipboardMonitor::ClipboardMonitor(QObject *parent)
    : QObject(parent)
    , clipboard(QApplication::clipboard())
    , timer(new QTimer(this))
{
    connect(timer, &QTimer::timeout, this, &ClipboardMonitor::checkClipboard);
    timer->setInterval(500);
}

void ClipboardMonitor::startMonitoring()
{
    checkClipboard();
    timer->start();
}

void ClipboardMonitor::stopMonitoring()
{
    timer->stop();
}

void ClipboardMonitor::checkClipboard()
{
    const QMimeData* mimeData = clipboard->mimeData();
    if (mimeData != nullptr && mimeData->hasUrls()) {
        QStringList filePaths;
        const auto urls = mimeData->urls();
        for (const QUrl& url : urls) {
            if (url.isLocalFile()) {
                filePaths.append(url.toLocalFile());
            }
        }

        const QString signature = QStringLiteral("files:") + filePaths.join(QStringLiteral("|"));
        if (!filePaths.isEmpty() && signature != lastSignature) {
            lastSignature = signature;
            emit filesChanged(filePaths);
            return;
        }
    }

    const QString currentContent = clipboard->text();
    const QString signature = QStringLiteral("text:") + currentContent;
    if (signature != lastSignature) {
        lastSignature = signature;
        emit textChanged(currentContent);
    }
}

void ClipboardMonitor::setClipboardText(const QString &content)
{
    if (content.isEmpty()) {
        return;
    }

    timer->stop();
    clipboard->setText(content);
    lastSignature = QStringLiteral("text:") + content;
    timer->start();
}
