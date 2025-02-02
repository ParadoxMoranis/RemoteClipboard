#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "clipboardmonitor.h"
#include "tcpclient.h"
#include <QMessageBox>
#include <QDebug>
#include <QJsonObject>
#include <QJsonDocument>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , clipboardMonitor(new ClipboardMonitor(this))
    , tcpClient(new TcpClient(this))
{
    ui->setupUi(this);
    setupConnections();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupConnections()
{
    connect(ui->btnConnect, &QPushButton::clicked,
            this, &MainWindow::onConnectClicked);
    
    connect(clipboardMonitor, &ClipboardMonitor::clipboardChanged,
            this, &MainWindow::onClipboardChanged);
    
    connect(tcpClient, &TcpClient::connected, this, &MainWindow::handleConnected);
    connect(tcpClient, &TcpClient::disconnected, this, &MainWindow::handleDisconnected);
    connect(tcpClient, &TcpClient::error, this, &MainWindow::handleError);
    connect(tcpClient, &TcpClient::dataReceived, this, &MainWindow::onDataReceived);
}

void MainWindow::onConnectClicked()
{
    if (ui->usernameEdit->text().isEmpty() || ui->passwordEdit->text().isEmpty()) {
        QMessageBox::warning(this, "Error", "Please enter username and password");
        return;
    }

    QString host = ui->serverAddressEdit->text();
    bool ok;
    quint16 port = ui->portEdit->text().toUShort(&ok);
    if (!ok || port < 1024 || port > 65535) {
        QMessageBox::warning(this, "Error", "Please enter a valid port number (1024-65535)");
        return;
    }
    
    updateStatus("Connecting to server...");
    tcpClient->connectToServer(host, port);
}

void MainWindow::onClipboardChanged(const QString &content)
{
    if (!tcpClient || !tcpClient->isConnected()) {
        return;
    }

    // Create a JSON object with the specified format
    QJsonObject clipboardData;
    clipboardData["content"] = content;  // 先添加content
    clipboardData["type"] = "clipboard"; // 再添加type

    // Convert to JSON and send
    QJsonDocument doc(clipboardData);
    QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
    tcpClient->sendData(jsonData);
    qDebug() << "Sent clipboard data:" << jsonData;
}

void MainWindow::handleConnected()
{
    qDebug() << "Connected to server";
    updateStatus("Connected to server");

    // 发送认证请求
    QJsonObject authRequest;
    authRequest["type"] = "auth";
    authRequest["username"] = ui->usernameEdit->text();
    authRequest["password"] = ui->passwordEdit->text();

    QJsonDocument doc(authRequest);
    QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
    tcpClient->sendData(jsonData);
    qDebug() << "Sent auth request:" << jsonData;

    ui->btnConnect->setEnabled(false);
}

void MainWindow::handleDisconnected()
{
    // 断开连接后的处理
    qDebug() << "Disconnected from server";
    ui->btnConnect->setEnabled(true);
    updateStatus("Disconnected from server");
}

void MainWindow::handleError(const QString& error)
{
    // 错误处理
    qDebug() << "Error:" << error;
    QMessageBox::critical(this, "错误", "发生错误：" + error);
    updateStatus("Error: " + error);
}

void MainWindow::onDataReceived(const QByteArray& data)
{
    qDebug() << "Received data:" << data;
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "JSON parse error:" << parseError.errorString();
        return;
    }

    if (!doc.isObject()) {
        qDebug() << "Received data is not a JSON object";
        return;
    }

    QJsonObject obj = doc.object();
    
    // 检查消息类型
    if (!obj.contains("type")) {
        qDebug() << "Received data has no type field";
        return;
    }

    QString type = obj["type"].toString();
    qDebug() << "Received message type:" << type;
    
    if (type == "clipboard") {
        if (!obj.contains("content")) {
            qDebug() << "Received clipboard data has no content field";
            return;
        }
        QString content = obj["content"].toString();
        if (!content.isEmpty()) {
            qDebug() << "Setting clipboard content:" << content;
            clipboardMonitor->setClipboardContent(content);
        }
    }
    else if (type == "auth_response") {
        handleAuthResponse(obj);
    }
}

void MainWindow::handleAuthResponse(const QJsonObject& response)
{
    qDebug() << "Handling auth response:" << QJsonDocument(response).toJson();
    
    // 检查响应中的字段
    if (!response.contains("success")) {
        qDebug() << "Auth response missing 'success' field";
        return;
    }

    bool success = response["success"].toBool();
    qDebug() << "Auth success value:" << success;

    if (success) {
        qDebug() << "Authentication successful";
        updateStatus("Authentication successful");
        // 认证成功后可以开始剪贴板监控
        clipboardMonitor->startMonitoring();
    } else {
        QString reason;
        if (response.contains("reason")) {
            reason = response["reason"].toString();
        } else if (response.contains("message")) {
            reason = response["message"].toString();
        } else {
            reason = "Unknown error";
        }
        
        qDebug() << "Authentication failed:" << reason;
        updateStatus("Authentication failed: " + reason);
        // 认证失败，重新启用连接按钮
        ui->btnConnect->setEnabled(true);
        // 断开连接
        tcpClient->disconnectFromServer();
    }
}

void MainWindow::updateStatus(const QString &message)
{
    ui->statusBar->showMessage(message);
    ui->logTextEdit->append(message);
}
