#include "mainwindow.h"
#include "clipboardmonitor.h"
#include "ui_mainwindow.h"

#include <QCheckBox>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>

namespace {
constexpr qint64 kMaxFileBundleBytes = 32ll * 1024ll * 1024ll;
constexpr auto kSettingsOrganization = "RemoteClipboard";
constexpr auto kSettingsApplication = "RemoteClipboardLinuxClient";
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , clipboardMonitor(new ClipboardMonitor(this))
    , tcpClient(new TcpClient(this))
    , tlsCheckBox(nullptr)
    , caCertificateEdit(nullptr)
    , receiveDirectoryEdit(nullptr)
{
    ui->setupUi(this);
    setupAdvancedControls();
    loadSettings();
    setupConnections();
    updateConnectButton();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupConnections()
{
    connect(ui->btnConnect, &QPushButton::clicked,
            this, &MainWindow::onConnectClicked);

    connect(clipboardMonitor, &ClipboardMonitor::textChanged,
            this, &MainWindow::onClipboardTextChanged);
    connect(clipboardMonitor, &ClipboardMonitor::filesChanged,
            this, &MainWindow::onClipboardFilesChanged);

    connect(tcpClient, &TcpClient::connected, this, &MainWindow::handleConnected);
    connect(tcpClient, &TcpClient::disconnected, this, &MainWindow::handleDisconnected);
    connect(tcpClient, &TcpClient::error, this, &MainWindow::handleError);
    connect(tcpClient, &TcpClient::authResponse, this, &MainWindow::handleAuthResponse);
    connect(tcpClient, &TcpClient::dataReceived, this, &MainWindow::onDataReceived);
    connect(tcpClient, &TcpClient::reconnectScheduled, this, &MainWindow::onReconnectScheduled);
}

void MainWindow::setupAdvancedControls()
{
    auto receiveDirectoryLabel = new QLabel(tr("Receive Dir:"), this);
    receiveDirectoryEdit = new QLineEdit(this);
    auto receiveDirectoryButton = new QPushButton(tr("Browse"), this);
    auto receiveDirectoryLayout = new QHBoxLayout();
    receiveDirectoryLayout->setContentsMargins(0, 0, 0, 0);
    receiveDirectoryLayout->addWidget(receiveDirectoryEdit);
    receiveDirectoryLayout->addWidget(receiveDirectoryButton);
    auto receiveDirectoryContainer = new QWidget(this);
    receiveDirectoryContainer->setLayout(receiveDirectoryLayout);

    auto tlsLabel = new QLabel(tr("Enable TLS:"), this);
    tlsCheckBox = new QCheckBox(tr("Use TLS (optional)"), this);

    auto caCertificateLabel = new QLabel(tr("CA Cert:"), this);
    caCertificateEdit = new QLineEdit(this);
    auto caCertificateButton = new QPushButton(tr("Browse"), this);
    auto caCertificateLayout = new QHBoxLayout();
    caCertificateLayout->setContentsMargins(0, 0, 0, 0);
    caCertificateLayout->addWidget(caCertificateEdit);
    caCertificateLayout->addWidget(caCertificateButton);
    auto caCertificateContainer = new QWidget(this);
    caCertificateContainer->setLayout(caCertificateLayout);

    ui->gridLayout->addWidget(receiveDirectoryLabel, 4, 0);
    ui->gridLayout->addWidget(receiveDirectoryContainer, 4, 1);
    ui->gridLayout->addWidget(tlsLabel, 5, 0);
    ui->gridLayout->addWidget(tlsCheckBox, 5, 1);
    ui->gridLayout->addWidget(caCertificateLabel, 6, 0);
    ui->gridLayout->addWidget(caCertificateContainer, 6, 1);
    ui->gridLayout->addWidget(ui->btnConnect, 7, 0, 1, 2);

    connect(receiveDirectoryButton, &QPushButton::clicked,
            this, &MainWindow::onBrowseReceiveDirectory);
    connect(caCertificateButton, &QPushButton::clicked,
            this, &MainWindow::onBrowseCaCertificate);
}

void MainWindow::loadSettings()
{
    QSettings settings(kSettingsOrganization, kSettingsApplication);
    ui->serverAddressEdit->setText(settings.value("connection/host", ui->serverAddressEdit->text()).toString());
    ui->portSpinBox->setValue(settings.value("connection/port", ui->portSpinBox->value()).toInt());
    ui->usernameEdit->setText(settings.value("connection/username").toString());
    tlsCheckBox->setChecked(settings.value("connection/use_tls", false).toBool());
    caCertificateEdit->setText(settings.value("connection/ca_cert").toString());
    receiveDirectoryEdit->setText(settings.value("files/receive_dir", defaultReceiveDirectory()).toString());
}

void MainWindow::saveSettings() const
{
    QSettings settings(kSettingsOrganization, kSettingsApplication);
    settings.setValue("connection/host", ui->serverAddressEdit->text());
    settings.setValue("connection/port", ui->portSpinBox->value());
    settings.setValue("connection/username", ui->usernameEdit->text());
    settings.setValue("connection/use_tls", tlsCheckBox->isChecked());
    settings.setValue("connection/ca_cert", caCertificateEdit->text());
    settings.setValue("files/receive_dir", receiveDirectoryEdit->text());
}

void MainWindow::onConnectClicked()
{
    if (connectionRequested) {
        connectionRequested = false;
        clipboardMonitor->stopMonitoring();
        tcpClient->disconnectFromServer();
        updateConnectButton();
        updateStatus(tr("Disconnect requested"));
        return;
    }

    if (ui->usernameEdit->text().isEmpty() || ui->passwordEdit->text().isEmpty()) {
        QMessageBox::warning(this, tr("Missing Credentials"), tr("Please enter username and password."));
        return;
    }

    QDir().mkpath(receiveDirectory());
    saveSettings();

    connectionRequested = true;
    updateConnectButton();
    updateStatus(tr("Connecting to server..."));

    tcpClient->connectToServer(
        ui->serverAddressEdit->text(),
        static_cast<quint16>(ui->portSpinBox->value()),
        tlsCheckBox->isChecked(),
        caCertificateEdit->text(),
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

    const QJsonObject message = buildFileBundleMessage(filePaths);
    if (message.isEmpty()) {
        return;
    }

    tcpClient->sendJson(message);
    updateStatus(tr("Sent %1 file(s) from clipboard").arg(filePaths.size()));
}

void MainWindow::handleConnected()
{
    updateStatus(tr("Transport connected, sending authentication"));
    QJsonObject authRequest;
    authRequest["type"] = "auth";
    authRequest["username"] = ui->usernameEdit->text();
    authRequest["password"] = ui->passwordEdit->text();
    tcpClient->sendJson(authRequest);
}

void MainWindow::handleDisconnected()
{
    clipboardMonitor->stopMonitoring();
    updateStatus(connectionRequested ? tr("Connection lost") : tr("Disconnected"));
    updateConnectButton();
}

void MainWindow::handleError(const QString& error)
{
    updateStatus(tr("Network error: %1").arg(error));
}

void MainWindow::handleAuthResponse(const QJsonObject& response)
{
    const bool success = response.value("status").toString() == "ok";
    if (success) {
        updateStatus(tr("Authenticated successfully"));
        clipboardMonitor->startMonitoring();
        return;
    }

    connectionRequested = false;
    clipboardMonitor->stopMonitoring();
    updateConnectButton();
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

void MainWindow::updateStatus(const QString &message)
{
    ui->statusBar->showMessage(message);
    ui->logTextEdit->append(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss ")) + message);
}

void MainWindow::updateConnectButton()
{
    ui->btnConnect->setText(connectionRequested ? tr("Disconnect") : tr("Connect"));
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
    return receiveDirectoryEdit->text().isEmpty()
        ? defaultReceiveDirectory()
        : receiveDirectoryEdit->text();
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
        if (totalBytes > kMaxFileBundleBytes) {
            QMessageBox::warning(const_cast<MainWindow*>(this),
                                 tr("File Bundle Too Large"),
                                 tr("Selected clipboard files exceed the 32MB safe transfer limit."));
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

void MainWindow::saveReceivedFiles(const QJsonObject& message)
{
    const QJsonArray files = message.value("files").toArray();
    if (files.isEmpty()) {
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

        QString targetPath = QDir(baseDir.filePath(batchName)).filePath(fileName);
        if (QFile::exists(targetPath)) {
            const QFileInfo info(targetPath);
            targetPath = info.dir().filePath(info.completeBaseName() + "-" +
                QDateTime::currentDateTime().toString(QStringLiteral("hhmmss")) +
                (info.suffix().isEmpty() ? QString() : QStringLiteral(".") + info.suffix()));
        }

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

QString MainWindow::sanitizeFileName(const QString& fileName) const
{
    QString result = fileName;
    const QString illegal = QStringLiteral("\\/:*?\"<>|");
    for (const QChar character : illegal) {
        result.replace(character, QLatin1Char('_'));
    }
    return result;
}
