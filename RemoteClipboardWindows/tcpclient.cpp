#include "tcpclient.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QDebug>

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
        qDebug() << "Sent data:" << data;
    }
}

void TcpClient::handleConnected()
{
    qDebug() << "Socket connected";
    emit connected();
}

void TcpClient::handleDisconnected()
{
    qDebug() << "Socket disconnected";
    emit disconnected();
}

void TcpClient::handleError(QAbstractSocket::SocketError socketError)
{
    QString errorStr = socket->errorString();
    qDebug() << "Socket error:" << errorStr;
    emit error(errorStr);
}

void TcpClient::handleReadyRead()
{
    QByteArray data = socket->readAll();
    qDebug() << "Received raw data:" << data;
    
    // 尝试解析JSON响应
    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "JSON parse error:" << parseError.errorString();
        emit dataReceived(data);
        return;
    }

    if (!jsonDoc.isObject()) {
        qDebug() << "Received data is not a JSON object";
        emit dataReceived(data);
        return;
    }

    QJsonObject jsonObj = jsonDoc.object();
    qDebug() << "Parsed JSON:" << QJsonDocument(jsonObj).toJson();
    
    // 检查消息类型
    if (!jsonObj.contains("type")) {
        qDebug() << "JSON missing type field";
        emit dataReceived(data);
        return;
    }

    QString type = jsonObj["type"].toString();
    qDebug() << "Message type:" << type;
    
    // 如果是认证响应，发送认证响应信号
    if (type == "auth_response") {
        emit authResponse(jsonObj);
    } else {
        // 其他类型的数据，发送数据接收信号
        emit dataReceived(data);
    }
}