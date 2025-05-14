#ifndef CLIPBOARDMONITOR_H
#define CLIPBOARDMONITOR_H

#include <QObject>
#include <QClipboard>
#include <QApplication>
#include <QTimer>

class ClipboardMonitor : public QObject
{
    Q_OBJECT
public:
    explicit ClipboardMonitor(QObject *parent = nullptr);
    void startMonitoring();
    void setClipboardContent(const QString &content);

signals:
    void clipboardChanged(const QString &content);

private slots:
    void checkClipboard();

private:
    QClipboard *clipboard;
    QString lastContent;
    QTimer *timer;
};

#endif // CLIPBOARDMONITOR_H