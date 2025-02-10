#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "tcpclient.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class ClipboardMonitor;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onConnectClicked();
    void onClipboardChanged(const QString &content);
    void handleConnected();
    void handleDisconnected();
    void handleAuthenticationSuccess();
    void handleAuthenticationFailed(const QString& reason);
    void handleError(const QString& error);
    void onDataReceived(const QByteArray& data);

private:
    Ui::MainWindow *ui;
    ClipboardMonitor *clipboardMonitor;
    TcpClient* tcpClient;

    void setupConnections();
    void updateStatus(const QString &message);
};

#endif // MAINWINDOW_H
