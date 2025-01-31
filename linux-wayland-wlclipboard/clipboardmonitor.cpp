#include "clipboardmonitor.h"
#include <QDebug>

ClipboardMonitor::ClipboardMonitor(QObject *parent)
    : QObject(parent)
    , checkTimer(new QTimer(this))
    , isMonitoring(false)
{
    // 设置定时器检查间隔（100毫秒，提高频率）
    checkTimer->setInterval(100);
    
    // 连接定时器超时信号
    connect(checkTimer, &QTimer::timeout, this, &ClipboardMonitor::checkClipboard);
}

ClipboardMonitor::~ClipboardMonitor()
{
    stopMonitoring();
}

void ClipboardMonitor::startMonitoring()
{
    if (!isMonitoring) {
        lastContent = getClipboardContent();
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

QString ClipboardMonitor::getClipboardContent()
{
    QProcess process;
    process.start("wl-paste", QStringList());
    process.waitForFinished();
    return QString::fromUtf8(process.readAllStandardOutput());
}

void ClipboardMonitor::setClipboardContent(const QString &content)
{
    if (content != lastContent) {
        setWlClipboard(content);
        lastContent = content;
    }
}

void ClipboardMonitor::setWlClipboard(const QString &content)
{
    QProcess process;
    process.start("wl-copy", QStringList());
    process.write(content.toUtf8());
    process.closeWriteChannel();
    process.waitForFinished();
}

void ClipboardMonitor::checkClipboard()
{
    QString currentContent = getClipboardContent();
    if (currentContent != lastContent) {
        lastContent = currentContent;
        emit clipboardChanged(currentContent);
    }
} 