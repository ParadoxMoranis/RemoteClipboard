#include "tcpclient.h"
#include <QJsonObject>
#include <QJsonDocument>

TcpClient::TcpClient(QObject *parent)
    : QObject(parent)
    , socket(new QTcpSocket(this))
{
    connect(socket, &QTcpSocket::connected, this, &TcpClient::handleConnected);
    connect(socket, &QTcpSocket::disconnected, this, &TcpClient::handleDisconnected);
    connect(socket, &QTcpSocket::errorOccurred, this, &TcpClient::handleError);
    connect(socket, &QTcpSocket::readyRead, this, &TcpClient::handleReadyRead);
}

void TcpClient::connectToServer(const QString& host, quint16 port)
{
    socket->connectToHost(host, port);
}

void TcpClient::disconnectFromServer()
{
    socket->disconnectFromHost();
}

bool TcpClient::isConnected() const
{
    return socket->state() == QAbstractSocket::ConnectedState;
}

void TcpClient::sendData(const QByteArray& data)
{
    if (isConnected()) {
        socket->write(data);
    }
}

void TcpClient::handleConnected()
{
    emit connected();
}

void TcpClient::handleDisconnected()
{
    emit disconnected();
}

void TcpClient::handleError(QAbstractSocket::SocketError socketError)
{
    emit error(socket->errorString());
}

void TcpClient::handleReadyRead()
{
    QByteArray data = socket->readAll();
    
    // 尝试解析JSON响应
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
    if (!jsonDoc.isNull() && jsonDoc.isObject()) {
        QJsonObject jsonObj = jsonDoc.object();
        
        // 检查是否是认证响应
        if (jsonObj["type"].toString() == "auth_response") {
            processAuthResponse(jsonObj);
        } else {
            // 其他类型的数据，发送信号给上层处理
            emit dataReceived(data);
        }
    } else {
        // 非JSON数据，直接发送信号
        emit dataReceived(data);
    }
}

void TcpClient::processAuthResponse(const QJsonObject& response)
{
    if (response["status"].toString() == "ok") {
        emit authenticationSuccess();
    } else {
        emit authenticationFailed(response["message"].toString());
    }
} 