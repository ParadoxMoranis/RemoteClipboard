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
    connect(tcpClient, &TcpClient::authenticationSuccess, this, &MainWindow::handleAuthenticationSuccess);
    connect(tcpClient, &TcpClient::authenticationFailed, this, &MainWindow::handleAuthenticationFailed);
    connect(tcpClient, &TcpClient::error, this, &MainWindow::handleError);
    connect(tcpClient, &TcpClient::dataReceived, this, &MainWindow::onDataReceived);
}

void MainWindow::onConnectClicked()
{
    QString host = ui->serverAddressEdit->text();
    quint16 port = ui->portSpinBox->value();
    
    if (ui->usernameEdit->text().isEmpty() || ui->passwordEdit->text().isEmpty()) {
        QMessageBox::warning(this, "错误", "请输入用户名和密码");
        return;
    }
    
    // 只进行TCP连接
    tcpClient->connectToServer(host, port);
}

void MainWindow::onClipboardChanged(const QString &content)
{
    // 创建JSON对象
    QJsonObject clipboardData;
    clipboardData["type"] = "clipboard";
    clipboardData["content"] = content;
    
    // 转换为JSON文档并发送
    QJsonDocument doc(clipboardData);
    tcpClient->sendData(doc.toJson());
    
    updateStatus("Sent clipboard content to server");
}

void MainWindow::handleConnected()
{
    // 连接成功后的处理
    qDebug() << "Connected to server";
    ui->btnConnect->setEnabled(false);
    updateStatus("Connected to server");

    // 在连接成功后发送认证信息
    QJsonObject authRequest;
    authRequest["type"] = "auth";
    authRequest["username"] = ui->usernameEdit->text();
    authRequest["password"] = ui->passwordEdit->text();
    
    QJsonDocument doc(authRequest);
    tcpClient->sendData(doc.toJson());
    
    updateStatus("Sent authentication request");
}

void MainWindow::handleDisconnected()
{
    // 断开连接后的处理
    qDebug() << "Disconnected from server";
    ui->btnConnect->setEnabled(true);
    updateStatus("Disconnected from server");
    
    // 断开连接时停止监控剪贴板
    clipboardMonitor->stopMonitoring();
}

void MainWindow::handleAuthenticationSuccess()
{
    // 认证成功后的处理
    qDebug() << "Authentication successful";
    QMessageBox::information(this, "认证成功", "服务器认证成功！");
    updateStatus("Authentication successful");
    
    // 认证成功后才开始监控剪贴板
    clipboardMonitor->startMonitoring();
}

void MainWindow::handleAuthenticationFailed(const QString& reason)
{
    // 认证失败后的处理
    qDebug() << "Authentication failed:" << reason;
    QMessageBox::warning(this, "认证失败", "服务器认证失败：" + reason);
    updateStatus("Authentication failed");
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
    // 尝试解析接收到的数据
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        // 检查是否是剪贴板类型的消息
        if (obj["type"].toString() == "clipboard" && obj.contains("content")) {
            QString content = obj["content"].toString();
            // 使用剪贴板监视器设置新内容
            clipboardMonitor->setClipboardContent(content);
            updateStatus("Received and set new clipboard content: " + content.left(50) + "...");
        }
    } else {
        qDebug() << "Received invalid JSON data";
    }
}

void MainWindow::updateStatus(const QString &message)
{
    ui->statusBar->showMessage(message);
    ui->logTextEdit->append(message);
}
