#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QJsonObject>
#include <QMainWindow>

#include "tcpclient.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class ClipboardMonitor;
class QCheckBox;
class QLineEdit;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onConnectClicked();
    void onClipboardTextChanged(const QString &content);
    void onClipboardFilesChanged(const QStringList& filePaths);
    void handleConnected();
    void handleDisconnected();
    void handleError(const QString& error);
    void handleAuthResponse(const QJsonObject& response);
    void onDataReceived(const QJsonObject& data);
    void onBrowseReceiveDirectory();
    void onBrowseCaCertificate();
    void onReconnectScheduled(int attempt, int delayMs);

private:
    void setupConnections();
    void setupAdvancedControls();
    void loadSettings();
    void saveSettings() const;
    void updateStatus(const QString &message);
    void updateConnectButton();
    QString defaultReceiveDirectory() const;
    QString receiveDirectory() const;
    QJsonObject buildFileBundleMessage(const QStringList& filePaths) const;
    void saveReceivedFiles(const QJsonObject& message);
    QString sanitizeFileName(const QString& fileName) const;

    Ui::MainWindow *ui;
    ClipboardMonitor *clipboardMonitor;
    TcpClient* tcpClient;
    QCheckBox* tlsCheckBox;
    QLineEdit* caCertificateEdit;
    QLineEdit* receiveDirectoryEdit;
    bool connectionRequested = false;
};

#endif // MAINWINDOW_H
