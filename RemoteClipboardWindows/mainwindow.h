#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QJsonObject>
#include <QMainWindow>
#include <QMap>
#include <QShortcut>
#include <QSystemTrayIcon>

#include "tcpclient.h"
#include "../gui_common/clientconfig.h"
#include "../gui_common/autostartmanager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class ClipboardMonitor;
class QCheckBox;
class QComboBox;
class QFile;
class QLabel;
class QLineEdit;
class QPushButton;
class QCloseEvent;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(bool autoStartMode = false, QWidget *parent = nullptr);
    ~MainWindow();

#ifdef Q_OS_WINDOWS
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
#endif
    void closeEvent(QCloseEvent* event) override;

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
    void onOpenSettings();
    void onProfileChanged(int index);
    void showMainWindow();
    void hideToTray();
    void switchToNextProfile();
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void onToggleColorScheme();

private:
    struct IncomingChunkTransfer {
        QString transferId;
        QString sender;
        QString fileName;
        QString sha256;
        QString mimeType;
        qint64 expectedSize = 0;
        qint64 writtenBytes = 0;
        QString targetPath;
        QFile* output = nullptr;
    };

    void setupConnections();
    void setupAdvancedControls();
    void setupTray();
    void setupShortcuts();
    void loadSettings();
    void saveSettings();
    void applyConfigToUi();
    void syncProfileFromUi();
    void selectProfileById(const QString& profileId);
    ConnectionProfile* currentProfile();
    const ConnectionProfile* currentProfile() const;
    void updateProfileSelector();
    void applyAutoStart();
    void updateStatus(const QString &message);
    void updateConnectButton();
    void applyLanguage();
    void applyColorScheme();
    void setConnectionState(const QString& label, const QString& state);
    QString defaultReceiveDirectory() const;
    QString receiveDirectory() const;
    bool ensureReceiveDirectoryReady();
    bool ensureDirectoryExists(const QString& path, bool interactive, QString* resolvedPath = nullptr);
    QJsonObject buildFileBundleMessage(const QStringList& filePaths) const;
    void saveReceivedFiles(const QJsonObject& message);
    bool sendChunkedFiles(const QStringList& filePaths);
    void handleChunkTransferMessage(const QJsonObject& data);
    void finalizeIncomingTransfer(const QString& transferId, bool success, const QString& reason = QString());
    QString sanitizeFileName(const QString& fileName) const;
    void reconnectUsingCurrentProfile(const QString& reason);
    void performAutoConnectIfNeeded();
    bool registerNativeHotkeys();
    void unregisterNativeHotkeys();

    Ui::MainWindow *ui;
    ClipboardMonitor *clipboardMonitor;
    TcpClient* tcpClient;
    QCheckBox* tlsCheckBox;
    QLineEdit* caCertificateEdit;
    QLineEdit* receiveDirectoryEdit;
    QPushButton* settingsButton;
    QPushButton* hideButton;
    QComboBox* profileComboBox;
    QPushButton* themeButton;
    QLabel* connectionStatusLabel;
    QSystemTrayIcon* trayIcon;
    QShortcut* showWindowShortcut;
    QShortcut* switchProfileShortcut;
    GuiConfigStore configStore;
    AutoStartManager autoStartManager;
    GuiAppConfig appConfig;
    QMap<QString, IncomingChunkTransfer> incomingTransfers;
    bool autoStartMode = false;
    bool suppressProfileChangeSignal = false;
    bool connectionRequested = false;
};

#endif // MAINWINDOW_H
