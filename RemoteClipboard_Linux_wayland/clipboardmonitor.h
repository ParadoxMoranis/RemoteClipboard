#ifndef CLIPBOARDMONITOR_H
#define CLIPBOARDMONITOR_H

#include <QObject>
#include <QTimer>

class ClipboardMonitor : public QObject
{
    Q_OBJECT
public:
    explicit ClipboardMonitor(QObject *parent = nullptr);
    ~ClipboardMonitor();

    void startMonitoring();
    void stopMonitoring();
    void setClipboardText(const QString &content);

signals:
    void textChanged(const QString &content);
    void filesChanged(const QStringList& filePaths);

private slots:
    void checkClipboard();

private:
    QString runCommand(const QString& program, const QStringList& arguments) const;
    QStringList readClipboardFiles() const;
    QString readClipboardText() const;
    QString buildSignatureForFiles(const QStringList& filePaths) const;

    QTimer *checkTimer;
    QString lastSignature;
    bool isMonitoring;
};

#endif // CLIPBOARDMONITOR_H
