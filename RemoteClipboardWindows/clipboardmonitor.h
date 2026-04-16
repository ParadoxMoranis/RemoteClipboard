#ifndef CLIPBOARDMONITOR_H
#define CLIPBOARDMONITOR_H

#include <QObject>
#include <QClipboard>
#include <QTimer>

class ClipboardMonitor : public QObject
{
    Q_OBJECT
public:
    explicit ClipboardMonitor(QObject *parent = nullptr);
    void startMonitoring();
    void stopMonitoring();
    void setClipboardText(const QString &content);

signals:
    void textChanged(const QString &content);
    void filesChanged(const QStringList& filePaths);

private slots:
    void checkClipboard();

private:
    QClipboard *clipboard;
    QString lastSignature;
    QTimer *timer;
};

#endif // CLIPBOARDMONITOR_H
