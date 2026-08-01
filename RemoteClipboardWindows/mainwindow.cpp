#include "mainwindow.h"

#include "clipboardmonitor.h"
#include "ui_mainwindow.h"
#include "../gui_common/uilanguage.h"
#include "../gui_common/settingsdialog.h"
#include "../gui_common/uitheme.h"

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QPushButton>
#include <QShortcut>
#include <QSizePolicy>
#include <QStandardPaths>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUuid>

#ifdef Q_OS_WINDOWS
#include <windows.h>
#endif

namespace {
constexpr qint64 kLegacyFileBundleLimitBytes = 4ll * 1024ll * 1024ll;
constexpr qint64 kChunkTransferThresholdBytes = 4ll * 1024ll * 1024ll;
constexpr qint64 kChunkSizeBytes = 512ll * 1024ll;
constexpr int kHotkeyShowWindowId = 1001;
constexpr int kHotkeySwitchProfileId = 1002;

QString uniqueFilePath(const QString& targetPath)
{
    if (!QFile::exists(targetPath)) {
        return targetPath;
    }

    QFileInfo info(targetPath);
    const QString base = info.completeBaseName();
    const QString suffix = info.suffix();

    for (int index = 1; index < 10000; ++index) {
        const QString candidate = info.dir().filePath(
            base + QStringLiteral("-%1").arg(index)
            + (suffix.isEmpty() ? QString() : QStringLiteral(".") + suffix));
        if (!QFile::exists(candidate)) {
            return candidate;
        }
    }

    return info.dir().filePath(
        base + QStringLiteral("-%1").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMddhhmmss")))
        + (suffix.isEmpty() ? QString() : QStringLiteral(".") + suffix));
}

#ifdef Q_OS_WINDOWS
bool keySequenceToNative(const QKeySequence& sequence, UINT& modifiers, UINT& virtualKey)
{
    if (sequence.isEmpty()) {
        return false;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QKeyCombination combination = sequence[0];
    const Qt::KeyboardModifiers qtModifiers = combination.keyboardModifiers();
    const int qtKey = combination.key();
#else
    const int combined = sequence[0];
    const Qt::KeyboardModifiers qtModifiers = Qt::KeyboardModifiers(combined & Qt::KeyboardModifierMask);
    const int qtKey = combined & ~Qt::KeyboardModifierMask;
#endif

    modifiers = 0;
    if (qtModifiers.testFlag(Qt::ShiftModifier)) {
        modifiers |= MOD_SHIFT;
    }
    if (qtModifiers.testFlag(Qt::ControlModifier)) {
        modifiers |= MOD_CONTROL;
    }
    if (qtModifiers.testFlag(Qt::AltModifier)) {
        modifiers |= MOD_ALT;
    }
    if (qtModifiers.testFlag(Qt::MetaModifier)) {
        modifiers |= MOD_WIN;
    }

    if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z) {
        virtualKey = static_cast<UINT>('A' + (qtKey - Qt::Key_A));
        return true;
    }
    if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9) {
        virtualKey = static_cast<UINT>('0' + (qtKey - Qt::Key_0));
        return true;
    }
    if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F24) {
        virtualKey = static_cast<UINT>(VK_F1 + (qtKey - Qt::Key_F1));
        return true;
    }

    switch (qtKey) {
    case Qt::Key_Space: virtualKey = VK_SPACE; return true;
    case Qt::Key_Tab: virtualKey = VK_TAB; return true;
    case Qt::Key_Return:
    case Qt::Key_Enter: virtualKey = VK_RETURN; return true;
    case Qt::Key_Escape: virtualKey = VK_ESCAPE; return true;
    case Qt::Key_Left: virtualKey = VK_LEFT; return true;
    case Qt::Key_Right: virtualKey = VK_RIGHT; return true;
    case Qt::Key_Up: virtualKey = VK_UP; return true;
    case Qt::Key_Down: virtualKey = VK_DOWN; return true;
    default:
        return false;
    }
}
#endif
}

MainWindow::MainWindow(bool autoStartMode, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , clipboardMonitor(new ClipboardMonitor(this))
    , tcpClient(new TcpClient(this))
    , tlsCheckBox(nullptr)
    , caCertificateEdit(nullptr)
    , receiveDirectoryEdit(nullptr)
    , settingsButton(nullptr)
    , hideButton(nullptr)
    , profileComboBox(nullptr)
    , themeButton(nullptr)
    , connectionStatusLabel(nullptr)
    , trayIcon(nullptr)
    , showWindowShortcut(nullptr)
    , switchProfileShortcut(nullptr)
    , configStore(QStringLiteral("RemoteClipboardWindowsClient"))
    , autoStartManager(QStringLiteral("RemoteClipboardWindowsClient"))
    , autoStartMode(autoStartMode)
{
    ui->setupUi(this);
    setupAdvancedControls();
    loadSettings();
    applyColorScheme();
    setupTray();
    setupConnections();
    setupShortcuts();
    updateConnectButton();
    setConnectionState(tr("● STANDBY"), QStringLiteral("idle"));
    performAutoConnectIfNeeded();
}

MainWindow::~MainWindow()
{
    unregisterNativeHotkeys();
    const QList<QString> transferIds = incomingTransfers.keys();
    for (const QString& transferId : transferIds) {
        finalizeIncomingTransfer(transferId, false);
    }
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (trayIcon != nullptr && trayIcon->isVisible()) {
        hideToTray();
        event->ignore();
        return;
    }
    QMainWindow::closeEvent(event);
}

#ifdef Q_OS_WINDOWS
bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
    Q_UNUSED(eventType);
    MSG* nativeMessage = static_cast<MSG*>(message);
    if (nativeMessage->message == WM_HOTKEY) {
        if (nativeMessage->wParam == kHotkeyShowWindowId) {
            showMainWindow();
            if (result != nullptr) {
                *result = 0;
            }
            return true;
        }
        if (nativeMessage->wParam == kHotkeySwitchProfileId) {
            switchToNextProfile();
            if (result != nullptr) {
                *result = 0;
            }
            return true;
        }
    }
    return false;
}
#endif

void MainWindow::setupConnections()
{
    connect(ui->btnConnect, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(settingsButton, &QPushButton::clicked, this, &MainWindow::onOpenSettings);
    connect(hideButton, &QPushButton::clicked, this, &MainWindow::hideToTray);
    connect(themeButton, &QPushButton::clicked, this, &MainWindow::onToggleColorScheme);
    connect(profileComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::onProfileChanged);

    connect(clipboardMonitor, &ClipboardMonitor::textChanged, this, &MainWindow::onClipboardTextChanged);
    connect(clipboardMonitor, &ClipboardMonitor::filesChanged, this, &MainWindow::onClipboardFilesChanged);

    connect(tcpClient, &TcpClient::connected, this, &MainWindow::handleConnected);
    connect(tcpClient, &TcpClient::disconnected, this, &MainWindow::handleDisconnected);
    connect(tcpClient, &TcpClient::error, this, &MainWindow::handleError);
    connect(tcpClient, &TcpClient::authResponse, this, &MainWindow::handleAuthResponse);
    connect(tcpClient, &TcpClient::dataReceived, this, &MainWindow::onDataReceived);
    connect(tcpClient, &TcpClient::reconnectScheduled, this, &MainWindow::onReconnectScheduled);
}

void MainWindow::setupAdvancedControls()
{
    profileComboBox = ui->profileComboBox;
    settingsButton = ui->settingsButton;
    hideButton = ui->hideButton;
    receiveDirectoryEdit = ui->receiveDirectoryEdit;
    tlsCheckBox = ui->tlsCheckBox;
    caCertificateEdit = ui->caCertificateEdit;
    themeButton = ui->themeButton;
    connectionStatusLabel = ui->connectionStatusLabel;
    settingsButton->setShortcut(QKeySequence(QStringLiteral("Ctrl+,")));

    const QList<QLabel*> fieldLabels = {
        ui->serverLabel,
        ui->portLabel,
        ui->usernameLabel,
        ui->passwordLabel,
        ui->profileLabel,
        ui->receiveDirectoryLabel,
        ui->caCertificateLabel
    };
    for (QLabel* label : fieldLabels) {
        label->setObjectName(QStringLiteral("fieldLabel"));
    }

    ui->clientGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    ui->gridLayout->setColumnStretch(1, 3);
    ui->gridLayout->setColumnStretch(3, 2);
    ui->verticalLayout->setStretchFactor(ui->logGroup, 1);

    connect(ui->receiveDirectoryButton, &QPushButton::clicked, this, &MainWindow::onBrowseReceiveDirectory);
    connect(ui->caCertificateButton, &QPushButton::clicked, this, &MainWindow::onBrowseCaCertificate);
}

void MainWindow::setupTray()
{
    if (trayIcon != nullptr) {
        trayIcon->hide();
        delete trayIcon;
        trayIcon = nullptr;
    }

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(windowIcon().isNull()
        ? style()->standardIcon(QStyle::SP_ComputerIcon)
        : windowIcon());

    auto* trayMenu = new QMenu(this);
    trayMenu->addAction(tr("Show"), this, &MainWindow::showMainWindow);
    trayMenu->addAction(tr("Switch Profile"), this, &MainWindow::switchToNextProfile);
    trayMenu->addSeparator();
    trayMenu->addAction(tr("Quit"), qApp, &QCoreApplication::quit);
    trayIcon->setContextMenu(trayMenu);
    trayIcon->show();

    connect(trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::onTrayActivated);
}

void MainWindow::setupShortcuts()
{
    if (showWindowShortcut != nullptr) {
        delete showWindowShortcut;
    }
    if (switchProfileShortcut != nullptr) {
        delete switchProfileShortcut;
    }

    showWindowShortcut = new QShortcut(appConfig.showWindowShortcut, this);
    connect(showWindowShortcut, &QShortcut::activated, this, &MainWindow::showMainWindow);

    switchProfileShortcut = new QShortcut(appConfig.switchProfileShortcut, this);
    connect(switchProfileShortcut, &QShortcut::activated, this, &MainWindow::switchToNextProfile);

    registerNativeHotkeys();
}

void MainWindow::loadSettings()
{
    appConfig = configStore.load();
    if (appConfig.receiveDirectory.isEmpty()) {
        appConfig.receiveDirectory = defaultReceiveDirectory();
    }
    applyLanguage();
    if (appConfig.autoStartEnabled != autoStartManager.isEnabled()) {
        applyAutoStart();
    }
    applyConfigToUi();
}

void MainWindow::saveSettings()
{
    syncProfileFromUi();
    appConfig.receiveDirectory = receiveDirectoryEdit->text().trimmed();
    if (const ConnectionProfile* profile = currentProfile()) {
        appConfig.selectedProfileId = profile->id;
    }

    QString errorMessage;
    if (!configStore.save(appConfig, &errorMessage)) {
        QMessageBox::warning(this, tr("Save Failed"), errorMessage);
    }

    applyAutoStart();
}

void MainWindow::applyConfigToUi()
{
    updateProfileSelector();
    receiveDirectoryEdit->setText(appConfig.receiveDirectory);
    selectProfileById(appConfig.selectedProfileId);
}

void MainWindow::syncProfileFromUi()
{
    ConnectionProfile* profile = currentProfile();
    if (profile == nullptr) {
        return;
    }

    profile->host = ui->serverAddressEdit->text().trimmed();
    profile->port = static_cast<quint16>(ui->portSpinBox->value());
    profile->username = ui->usernameEdit->text().trimmed();
    profile->password = ui->passwordEdit->text();
    profile->useTls = tlsCheckBox->isChecked();
    profile->caCertificatePath = caCertificateEdit->text().trimmed();
}

void MainWindow::selectProfileById(const QString& profileId)
{
    suppressProfileChangeSignal = true;
    const int index = profileComboBox->findData(profileId);
    profileComboBox->setCurrentIndex(index >= 0 ? index : 0);
    suppressProfileChangeSignal = false;
    onProfileChanged(profileComboBox->currentIndex());
}

ConnectionProfile* MainWindow::currentProfile()
{
    if (profileComboBox == nullptr || profileComboBox->currentIndex() < 0) {
        return nullptr;
    }
    return GuiConfigStore::findProfileById(appConfig, profileComboBox->currentData().toString());
}

const ConnectionProfile* MainWindow::currentProfile() const
{
    if (profileComboBox == nullptr || profileComboBox->currentIndex() < 0) {
        return nullptr;
    }
    return GuiConfigStore::findProfileById(appConfig, profileComboBox->currentData().toString());
}

void MainWindow::updateProfileSelector()
{
    suppressProfileChangeSignal = true;
    profileComboBox->clear();
    for (const ConnectionProfile& profile : appConfig.profiles) {
        profileComboBox->addItem(profile.name, profile.id);
    }
    suppressProfileChangeSignal = false;
}

void MainWindow::applyAutoStart()
{
    QString errorMessage;
    if (!autoStartManager.setEnabled(appConfig.autoStartEnabled, &errorMessage) && !errorMessage.isEmpty()) {
        updateStatus(tr("Autostart setup failed: %1").arg(errorMessage));
    }
}

void MainWindow::updateStatus(const QString &message)
{
    ui->statusBar->showMessage(message);
    ui->logTextEdit->append(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss ")) + message);
}

void MainWindow::updateConnectButton()
{
    ui->btnConnect->setText(connectionRequested ? tr("DISCONNECT // STOP") : tr("CONNECT // START"));
    ui->btnConnect->setProperty("active", connectionRequested);
    ui->btnConnect->style()->unpolish(ui->btnConnect);
    ui->btnConnect->style()->polish(ui->btnConnect);
}

void MainWindow::applyLanguage()
{
    appConfig.language = normalizedUiLanguage(appConfig.language);
    applyUiLanguage(*qApp, appConfig.language);
    ui->retranslateUi(this);
    updateConnectButton();

    if (tcpClient->isAuthenticated()) {
        setConnectionState(tr("● SYNC ONLINE"), QStringLiteral("online"));
    } else if (connectionRequested) {
        setConnectionState(tr("◆ CONNECTING"), QStringLiteral("pending"));
    } else {
        setConnectionState(tr("● STANDBY"), QStringLiteral("idle"));
    }
}

void MainWindow::applyColorScheme()
{
    const UiColorScheme scheme = uiColorSchemeFromName(appConfig.colorScheme);
    applyUiColorScheme(*qApp, scheme);
    themeButton->setText(scheme == UiColorScheme::Dark ? tr("LIGHT MODE ↗") : tr("DARK MODE ↗"));
}

void MainWindow::setConnectionState(const QString& label, const QString& state)
{
    connectionStatusLabel->setText(label);
    connectionStatusLabel->setProperty("connectionState", state);
    connectionStatusLabel->style()->unpolish(connectionStatusLabel);
    connectionStatusLabel->style()->polish(connectionStatusLabel);
}

void MainWindow::onToggleColorScheme()
{
    const UiColorScheme current = uiColorSchemeFromName(appConfig.colorScheme);
    appConfig.colorScheme = uiColorSchemeName(
        current == UiColorScheme::Dark ? UiColorScheme::Light : UiColorScheme::Dark);
    applyColorScheme();
    saveSettings();
}

QString MainWindow::defaultReceiveDirectory() const
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (base.isEmpty()) {
        base = QDir::homePath();
    }
    return QDir(base).filePath(QStringLiteral("RemoteClipboard"));
}

QString MainWindow::receiveDirectory() const
{
    const QString path = receiveDirectoryEdit->text().trimmed();
    return path.isEmpty() ? defaultReceiveDirectory() : path;
}

bool MainWindow::ensureReceiveDirectoryReady()
{
    QString resolved;
    return ensureDirectoryExists(receiveDirectory(), true, &resolved);
}

bool MainWindow::ensureDirectoryExists(const QString& path, bool interactive, QString* resolvedPath)
{
    const QString normalized = QDir::fromNativeSeparators(path.trimmed().isEmpty() ? defaultReceiveDirectory() : path.trimmed());
    QFileInfo info(normalized);
    if (info.exists() && info.isDir()) {
        if (resolvedPath != nullptr) {
            *resolvedPath = info.absoluteFilePath();
        }
        if (interactive) {
            QMessageBox::information(this, tr("Directory Ready"),
                tr("Receive directory found:\n%1").arg(info.absoluteFilePath()));
        }
        return true;
    }

    if (!interactive) {
        return QDir().mkpath(normalized);
    }

    const auto answer = QMessageBox::question(
        this,
        tr("Create Directory"),
        tr("The directory does not exist:\n%1\n\nCreate it now?").arg(normalized));
    if (answer != QMessageBox::Yes) {
        updateStatus(tr("Receive directory was not created"));
        return false;
    }

    const bool created = QDir().mkpath(normalized);
    if (created) {
        QMessageBox::information(this, tr("Directory Created"),
            tr("Directory created successfully:\n%1").arg(QFileInfo(normalized).absoluteFilePath()));
        if (resolvedPath != nullptr) {
            *resolvedPath = QFileInfo(normalized).absoluteFilePath();
        }
        return true;
    }

    QMessageBox::warning(this, tr("Create Failed"),
        tr("Failed to create directory:\n%1").arg(normalized));
    return false;
}

QJsonObject MainWindow::buildFileBundleMessage(const QStringList& filePaths) const
{
    QJsonArray files;
    qint64 totalBytes = 0;
    QMimeDatabase mimeDatabase;

    for (const QString& filePath : filePaths) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }

        const QByteArray fileData = file.readAll();
        totalBytes += fileData.size();
        if (totalBytes > kLegacyFileBundleLimitBytes) {
            QMessageBox::warning(const_cast<MainWindow*>(this),
                                 tr("File Bundle Too Large"),
                                 tr("Selected clipboard files exceed the small-file transfer threshold."));
            return {};
        }

        QFileInfo fileInfo(file);
        QJsonObject fileObject;
        fileObject["name"] = fileInfo.fileName();
        fileObject["size"] = static_cast<qint64>(fileData.size());
        fileObject["mime"] = mimeDatabase.mimeTypeForFile(fileInfo).name();
        fileObject["sha256"] = QString::fromLatin1(
            QCryptographicHash::hash(fileData, QCryptographicHash::Sha256).toHex());
        fileObject["data"] = QString::fromLatin1(fileData.toBase64());
        files.append(fileObject);
    }

    if (files.isEmpty()) {
        return {};
    }

    QJsonObject message;
    message["type"] = "file_bundle";
    message["files"] = files;
    return message;
}

bool MainWindow::sendChunkedFiles(const QStringList& filePaths)
{
    QMimeDatabase mimeDatabase;

    for (const QString& filePath : filePaths) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            updateStatus(tr("Skipped unreadable file: %1").arg(filePath));
            continue;
        }

        const QFileInfo info(file);
        const QString transferId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QByteArray fileHash = QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256);
        file.seek(0);

        QJsonObject startMessage;
        startMessage["type"] = "file_transfer_start";
        startMessage["transfer_id"] = transferId;
        startMessage["name"] = info.fileName();
        startMessage["size"] = static_cast<qint64>(file.size());
        startMessage["mime"] = mimeDatabase.mimeTypeForFile(info).name();
        startMessage["sha256"] = QString::fromLatin1(fileHash.toHex());
        tcpClient->sendJson(startMessage);

        int sequence = 0;
        while (!file.atEnd()) {
            const QByteArray chunk = file.read(kChunkSizeBytes);
            if (chunk.isEmpty() && file.size() > 0) {
                break;
            }

            QJsonObject chunkMessage;
            chunkMessage["type"] = "file_transfer_chunk";
            chunkMessage["transfer_id"] = transferId;
            chunkMessage["seq"] = sequence++;
            chunkMessage["data"] = QString::fromLatin1(chunk.toBase64());
            tcpClient->sendJson(chunkMessage);
        }

        QJsonObject completeMessage;
        completeMessage["type"] = "file_transfer_complete";
        completeMessage["transfer_id"] = transferId;
        completeMessage["chunk_size"] = kChunkSizeBytes;
        tcpClient->sendJson(completeMessage);
    }

    updateStatus(tr("Sent %1 file(s) using chunked transfer").arg(filePaths.size()));
    return true;
}

void MainWindow::saveReceivedFiles(const QJsonObject& message)
{
    const QJsonArray files = message.value("files").toArray();
    if (files.isEmpty()) {
        return;
    }

    if (!ensureDirectoryExists(receiveDirectory(), false)) {
        updateStatus(tr("Receive directory is unavailable, skipped incoming files"));
        return;
    }

    const QString sender = sanitizeFileName(message.value("sender").toString(QStringLiteral("peer")));
    const QString batchName = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-")) + sender;
    const QDir baseDir(receiveDirectory());
    QDir().mkpath(baseDir.filePath(batchName));

    int savedCount = 0;
    for (const QJsonValue& value : files) {
        const QJsonObject fileObject = value.toObject();
        const QString fileName = sanitizeFileName(fileObject.value("name").toString(QStringLiteral("clipboard-file")));
        const QByteArray fileData = QByteArray::fromBase64(fileObject.value("data").toString().toLatin1());
        if (fileData.isEmpty() && fileObject.value("size").toInt() > 0) {
            continue;
        }

        const QString targetPath = uniqueFilePath(QDir(baseDir.filePath(batchName)).filePath(fileName));
        QFile output(targetPath);
        if (!output.open(QIODevice::WriteOnly)) {
            continue;
        }
        output.write(fileData);
        output.close();
        ++savedCount;
    }

    updateStatus(tr("Saved %1 file(s) to %2").arg(savedCount).arg(baseDir.filePath(batchName)));
}

void MainWindow::handleChunkTransferMessage(const QJsonObject& data)
{
    const QString type = data.value("type").toString();
    const QString transferId = data.value("transfer_id").toString();
    if (transferId.isEmpty()) {
        return;
    }

    if (type == "file_transfer_start") {
        if (!ensureDirectoryExists(receiveDirectory(), false)) {
            updateStatus(tr("Receive directory is unavailable, skipped chunked transfer"));
            return;
        }

        finalizeIncomingTransfer(transferId, false);

        IncomingChunkTransfer transfer;
        transfer.transferId = transferId;
        transfer.sender = sanitizeFileName(data.value("sender").toString(QStringLiteral("peer")));
        transfer.fileName = sanitizeFileName(data.value("name").toString(QStringLiteral("clipboard-file")));
        transfer.sha256 = data.value("sha256").toString();
        transfer.mimeType = data.value("mime").toString();
        transfer.expectedSize = static_cast<qint64>(data.value("size").toDouble(0));

        const QString batchName = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-")) + transfer.sender;
        const QDir baseDir(receiveDirectory());
        QDir().mkpath(baseDir.filePath(batchName));
        transfer.targetPath = uniqueFilePath(QDir(baseDir.filePath(batchName)).filePath(transfer.fileName));
        transfer.output = new QFile(transfer.targetPath);
        if (!transfer.output->open(QIODevice::WriteOnly)) {
            delete transfer.output;
            transfer.output = nullptr;
            updateStatus(tr("Failed to open file for incoming transfer: %1").arg(transfer.targetPath));
            return;
        }

        incomingTransfers.insert(transferId, transfer);
        updateStatus(tr("Receiving file: %1").arg(transfer.fileName));
        return;
    }

    if (!incomingTransfers.contains(transferId)) {
        return;
    }

    IncomingChunkTransfer& transfer = incomingTransfers[transferId];
    if (type == "file_transfer_chunk") {
        const QByteArray chunk = QByteArray::fromBase64(data.value("data").toString().toLatin1());
        if (transfer.output == nullptr || !transfer.output->isOpen()) {
            finalizeIncomingTransfer(transferId, false, tr("Transfer target is not writable"));
            return;
        }
        if (transfer.output->write(chunk) != chunk.size()) {
            finalizeIncomingTransfer(transferId, false, tr("Failed while writing incoming chunk"));
            return;
        }
        transfer.writtenBytes += chunk.size();
        return;
    }

    if (type == "file_transfer_complete") {
        finalizeIncomingTransfer(transferId, true);
    }
}

void MainWindow::finalizeIncomingTransfer(const QString& transferId, bool success, const QString& reason)
{
    auto it = incomingTransfers.find(transferId);
    if (it == incomingTransfers.end()) {
        return;
    }

    IncomingChunkTransfer transfer = it.value();
    incomingTransfers.erase(it);

    if (transfer.output != nullptr) {
        if (transfer.output->isOpen()) {
            transfer.output->close();
        }
        delete transfer.output;
        transfer.output = nullptr;
    }

    if (!success) {
        if (!transfer.targetPath.isEmpty()) {
            QFile::remove(transfer.targetPath);
        }
        if (!reason.isEmpty()) {
            updateStatus(reason);
        }
        return;
    }

    QFile savedFile(transfer.targetPath);
    if (!savedFile.open(QIODevice::ReadOnly)) {
        updateStatus(tr("Saved file but failed to verify: %1").arg(transfer.targetPath));
        return;
    }
    const QByteArray savedData = savedFile.readAll();
    savedFile.close();

    if (transfer.expectedSize > 0 && savedData.size() != transfer.expectedSize) {
        QFile::remove(transfer.targetPath);
        updateStatus(tr("Discarded file with mismatched size: %1").arg(transfer.fileName));
        return;
    }

    if (!transfer.sha256.isEmpty()) {
        const QString actualHash = QString::fromLatin1(
            QCryptographicHash::hash(savedData, QCryptographicHash::Sha256).toHex());
        if (actualHash != transfer.sha256) {
            QFile::remove(transfer.targetPath);
            updateStatus(tr("Discarded file with mismatched hash: %1").arg(transfer.fileName));
            return;
        }
    }

    updateStatus(tr("Saved chunked file to %1").arg(transfer.targetPath));
}

QString MainWindow::sanitizeFileName(const QString& fileName) const
{
    QString result = fileName;
    const QString illegal = QStringLiteral("\\/:*?\"<>|");
    for (const QChar character : illegal) {
        result.replace(character, QLatin1Char('_'));
    }
    return result;
}

void MainWindow::onConnectClicked()
{
    if (connectionRequested) {
        connectionRequested = false;
        clipboardMonitor->stopMonitoring();
        tcpClient->disconnectFromServer();
        updateConnectButton();
        setConnectionState(tr("● STANDBY"), QStringLiteral("idle"));
        updateStatus(tr("Disconnect requested"));
        return;
    }

    syncProfileFromUi();
    if (ui->usernameEdit->text().trimmed().isEmpty() || ui->passwordEdit->text().isEmpty()) {
        QMessageBox::warning(this, tr("Missing Credentials"), tr("Please enter username and password."));
        return;
    }

    if (!ensureReceiveDirectoryReady()) {
        return;
    }

    saveSettings();
    connectionRequested = true;
    updateConnectButton();
    setConnectionState(tr("◆ CONNECTING"), QStringLiteral("pending"));
    updateStatus(tr("Connecting to server..."));

    tcpClient->connectToServer(
        ui->serverAddressEdit->text().trimmed(),
        static_cast<quint16>(ui->portSpinBox->value()),
        tlsCheckBox->isChecked(),
        caCertificateEdit->text().trimmed(),
        true
    );
}

void MainWindow::onClipboardTextChanged(const QString &content)
{
    if (!tcpClient->isAuthenticated()) {
        return;
    }

    QJsonObject message;
    message["type"] = "clipboard_text";
    message["content"] = content;
    tcpClient->sendJson(message);
    updateStatus(tr("Sent text clipboard update"));
}

void MainWindow::onClipboardFilesChanged(const QStringList& filePaths)
{
    if (!tcpClient->isAuthenticated()) {
        return;
    }

    qint64 totalBytes = 0;
    for (const QString& filePath : filePaths) {
        totalBytes += QFileInfo(filePath).size();
    }

    if (totalBytes >= kChunkTransferThresholdBytes) {
        sendChunkedFiles(filePaths);
        return;
    }

    const QJsonObject message = buildFileBundleMessage(filePaths);
    if (message.isEmpty()) {
        sendChunkedFiles(filePaths);
        return;
    }

    tcpClient->sendJson(message);
    updateStatus(tr("Sent %1 file(s) from clipboard").arg(filePaths.size()));
}

void MainWindow::handleConnected()
{
    setConnectionState(tr("◆ AUTHENTICATING"), QStringLiteral("pending"));
    updateStatus(tr("Transport connected, sending authentication"));
    QJsonObject authRequest;
    authRequest["type"] = "auth";
    authRequest["username"] = ui->usernameEdit->text().trimmed();
    authRequest["password"] = ui->passwordEdit->text();
    tcpClient->sendJson(authRequest);
}

void MainWindow::handleDisconnected()
{
    clipboardMonitor->stopMonitoring();
    setConnectionState(connectionRequested ? tr("× RECONNECTING") : tr("● STANDBY"),
        connectionRequested ? QStringLiteral("error") : QStringLiteral("idle"));
    updateStatus(connectionRequested ? tr("Connection lost") : tr("Disconnected"));
    updateConnectButton();
}

void MainWindow::handleError(const QString& error)
{
    setConnectionState(tr("× NETWORK ERROR"), QStringLiteral("error"));
    updateStatus(tr("Network error: %1").arg(error));
}

void MainWindow::handleAuthResponse(const QJsonObject& response)
{
    const bool success = response.value("status").toString() == "ok";
    if (success) {
        setConnectionState(tr("● SYNC ONLINE"), QStringLiteral("online"));
        updateStatus(tr("Authenticated successfully"));
        clipboardMonitor->startMonitoring();
        if (const ConnectionProfile* profile = currentProfile()) {
            appConfig.lastConnectedProfileId = profile->id;
            saveSettings();
        }
        if (autoStartMode && appConfig.autoConnectLastProfile) {
            trayIcon->showMessage(tr("Remote Clipboard"),
                tr("Auto-start connected using the last saved server profile."),
                QSystemTrayIcon::Information,
                3000);
        }
        return;
    }

    connectionRequested = false;
    clipboardMonitor->stopMonitoring();
    updateConnectButton();
    setConnectionState(tr("× AUTH FAILED"), QStringLiteral("error"));
    QMessageBox::warning(this, tr("Authentication Failed"),
                         response.value("message").toString(tr("Unknown error")));
    tcpClient->disconnectFromServer();
}

void MainWindow::onDataReceived(const QJsonObject& data)
{
    const QString type = data.value("type").toString();
    if (type == "clipboard_text") {
        clipboardMonitor->setClipboardText(data.value("content").toString());
        updateStatus(tr("Received text clipboard update"));
        return;
    }

    if (type == "file_bundle") {
        saveReceivedFiles(data);
        return;
    }

    if (type == "file_transfer_start" || type == "file_transfer_chunk" || type == "file_transfer_complete") {
        handleChunkTransferMessage(data);
    }
}

void MainWindow::onBrowseReceiveDirectory()
{
    const QString selected = QFileDialog::getExistingDirectory(
        this,
        tr("Select Receive Directory"),
        receiveDirectory()
    );
    if (!selected.isEmpty()) {
        receiveDirectoryEdit->setText(selected);
        saveSettings();
    }
}

void MainWindow::onBrowseCaCertificate()
{
    const QString selected = QFileDialog::getOpenFileName(
        this,
        tr("Select CA Certificate"),
        caCertificateEdit->text(),
        tr("Certificate Files (*.pem *.crt *.cer);;All Files (*)")
    );
    if (!selected.isEmpty()) {
        caCertificateEdit->setText(selected);
        saveSettings();
    }
}

void MainWindow::onReconnectScheduled(int attempt, int delayMs)
{
    if (!connectionRequested) {
        return;
    }

    updateStatus(tr("Reconnect attempt %1 scheduled in %2 seconds")
        .arg(attempt)
        .arg(delayMs / 1000.0, 0, 'f', 1));
}

void MainWindow::onOpenSettings()
{
    syncProfileFromUi();
    appConfig.receiveDirectory = receiveDirectoryEdit->text().trimmed();

    SettingsDialog dialog(appConfig, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    appConfig = dialog.config();
    if (appConfig.receiveDirectory.isEmpty()) {
        appConfig.receiveDirectory = defaultReceiveDirectory();
    }

    saveSettings();
    applyLanguage();
    applyColorScheme();
    applyConfigToUi();
    setupTray();
    setupShortcuts();
    updateStatus(tr("Settings updated. Config file: %1").arg(configStore.configPath()));
}

void MainWindow::onProfileChanged(int index)
{
    if (suppressProfileChangeSignal || index < 0) {
        return;
    }

    const QString profileId = profileComboBox->itemData(index).toString();
    ConnectionProfile* profile = GuiConfigStore::findProfileById(appConfig, profileId);
    if (profile == nullptr) {
        return;
    }

    ui->serverAddressEdit->setText(profile->host);
    ui->portSpinBox->setValue(profile->port);
    ui->usernameEdit->setText(profile->username);
    ui->passwordEdit->setText(profile->password);
    tlsCheckBox->setChecked(profile->useTls);
    caCertificateEdit->setText(profile->caCertificatePath);
    appConfig.selectedProfileId = profile->id;
}

void MainWindow::showMainWindow()
{
    showNormal();
    raise();
    activateWindow();
}

void MainWindow::hideToTray()
{
    hide();
    if (trayIcon != nullptr) {
        trayIcon->showMessage(tr("Remote Clipboard"),
            tr("The client is still running in the background."),
            QSystemTrayIcon::Information,
            2500);
    }
}

void MainWindow::switchToNextProfile()
{
    if (profileComboBox == nullptr || profileComboBox->count() <= 1) {
        return;
    }

    const int nextIndex = (profileComboBox->currentIndex() + 1) % profileComboBox->count();
    profileComboBox->setCurrentIndex(nextIndex);
    saveSettings();
    updateStatus(tr("Switched to profile: %1").arg(profileComboBox->currentText()));
}

void MainWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
        showMainWindow();
    }
}

void MainWindow::reconnectUsingCurrentProfile(const QString& reason)
{
    Q_UNUSED(reason);
    if (!connectionRequested) {
        connectionRequested = true;
        updateConnectButton();
    }
    QTimer::singleShot(0, this, &MainWindow::onConnectClicked);
}

void MainWindow::performAutoConnectIfNeeded()
{
    if (!appConfig.autoConnectLastProfile) {
        return;
    }

    const QString preferredProfileId = autoStartMode && !appConfig.lastConnectedProfileId.isEmpty()
        ? appConfig.lastConnectedProfileId
        : appConfig.selectedProfileId;
    selectProfileById(preferredProfileId);

    if (autoStartMode) {
        QTimer::singleShot(500, this, [this]() {
            if (ensureDirectoryExists(receiveDirectory(), false)) {
                onConnectClicked();
            }
        });
    }
}

bool MainWindow::registerNativeHotkeys()
{
#ifdef Q_OS_WINDOWS
    unregisterNativeHotkeys();

    UINT modifiers = 0;
    UINT virtualKey = 0;
    if (keySequenceToNative(appConfig.showWindowShortcut, modifiers, virtualKey)) {
        RegisterHotKey(reinterpret_cast<HWND>(winId()), kHotkeyShowWindowId, modifiers, virtualKey);
    }

    modifiers = 0;
    virtualKey = 0;
    if (keySequenceToNative(appConfig.switchProfileShortcut, modifiers, virtualKey)) {
        RegisterHotKey(reinterpret_cast<HWND>(winId()), kHotkeySwitchProfileId, modifiers, virtualKey);
    }
#endif
    return true;
}

void MainWindow::unregisterNativeHotkeys()
{
#ifdef Q_OS_WINDOWS
    UnregisterHotKey(reinterpret_cast<HWND>(winId()), kHotkeyShowWindowId);
    UnregisterHotKey(reinterpret_cast<HWND>(winId()), kHotkeySwitchProfileId);
#endif
}
