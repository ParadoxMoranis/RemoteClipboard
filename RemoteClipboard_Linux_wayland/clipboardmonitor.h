#ifndef CLIPBOARDMONITOR_H
#define CLIPBOARDMONITOR_H

#include <QObject>
#include <QProcess>
#include <QTimer>

class ClipboardMonitor : public QObject
{
    Q_OBJECT
public:
    explicit ClipboardMonitor(QObject *parent = nullptr);
    ~ClipboardMonitor();

    void startMonitoring();
    void stopMonitoring();
    void setClipboardContent(const QString &content);

signals:
    void clipboardChanged(const QString &content);

private slots:
    void checkClipboard();

private:
    QString getClipboardContent();
    void setWlClipboard(const QString &content);
    
    QTimer *checkTimer;
    QString lastContent;
    bool isMonitoring;
};

#endif // CLIPBOARDMONITOR_H 