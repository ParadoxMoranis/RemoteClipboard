#include "clipboardmonitor.h"
#include <QDebug>

ClipboardMonitor::ClipboardMonitor(QObject *parent)
    : QObject(parent)
    , clipboard(QApplication::clipboard())
    , timer(new QTimer(this))
{
    connect(timer, &QTimer::timeout, this, &ClipboardMonitor::checkClipboard);
    timer->setInterval(100); // Check every 100ms
}

void ClipboardMonitor::startMonitoring()
{
    lastContent = clipboard->text();
    timer->start();
}

void ClipboardMonitor::checkClipboard()
{
    QString currentContent = clipboard->text();
    if (currentContent != lastContent) {
        lastContent = currentContent;
        emit clipboardChanged(currentContent);
    }
}

void ClipboardMonitor::setClipboardContent(const QString &content)
{
    if (content.isEmpty()) {
        return;
    }

    // 暂时停止监听以避免循环
    timer->stop();
    
    // 设置新的剪贴板内容
    clipboard->setText(content);
    lastContent = content;
    
    // 恢复监听
    timer->start();
    
    qDebug() << "Clipboard content updated:" << content;
}