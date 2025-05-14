#include "tcpserver.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QCryptographicHash>
#include <QUuid>

TcpServer::TcpServer(QObject *parent)
    : QObject(parent)
    , server(new QTcpServer(this))
{
}

bool TcpServer::startServer(quint16 port)
{
    if (!server->listen(QHostAddress::Any, port)) {
        emit errorOccurred(server->errorString());
        return false;
    }

    connect(server, &QTcpServer::newConnection,
            this, &TcpServer::handleNewConnection);
    return true;
}

void TcpServer::stopServer()
{
    if (server->isListening()) {
        server->close();
    }

    for (QTcpSocket* socket : clients.keys()) {
        socket->disconnectFromHost();
    }
}

void TcpServer::setCredentials(const QString &username, const QString &password)
{
    serverUsername = username;
    serverPassword = password;
}

void TcpServer::handleNewConnection()
{
    QTcpSocket *clientSocket = server->nextPendingConnection();
    QString clientId = QUuid::createUuid().toString();
    
    connect(clientSocket, &QTcpSocket::disconnected,
            this, &TcpServer::handleClientDisconnected);
    connect(clientSocket, &QTcpSocket::readyRead,
            this, &TcpServer::handleReadyRead);
}

void TcpServer::handleClientDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        QString clientId = clients.value(socket);
        emit clientDisconnected(clientId);
        clients.remove(socket);
        socket->deleteLater();
    }
}

void TcpServer::handleReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    QByteArray data = socket->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    
    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        
        // 处理认证请求
        if (obj["type"].toString() == "auth") {
            if (validateCredentials(obj["username"].toString(), obj["password"].toString())) {
                clients[socket] = obj["username"].toString();
                QJsonObject response;
                response["type"] = "auth_response";
                response["success"] = true;
                socket->write(QJsonDocument(response).toJson());
                emit clientConnected(obj["username"].toString());
            } else {
                QJsonObject response;
                response["type"] = "auth_response";
                response["success"] = false;
                socket->write(QJsonDocument(response).toJson());
                socket->disconnectFromHost();
            }
        }
        // 处理剪贴板数据
        else if (obj["type"].toString() == "clipboard" && clients.contains(socket)) {
            QString content = obj["content"].toString();
            emit dataReceived(clients[socket], content);
            broadcastToOthers(socket, content);
        }
    }
}

void TcpServer::broadcastToOthers(QTcpSocket *sender, const QString &data)
{
    QJsonObject response;
    response["type"] = "clipboard";
    response["content"] = data;
    
    QJsonDocument doc(response);
    QByteArray jsonData = doc.toJson();

    for (QTcpSocket* socket : clients.keys()) {
        if (socket != sender) {
            socket->write(jsonData);
            socket->flush();
        }
    }
}

bool TcpServer::validateCredentials(const QString &username, const QString &password)
{
    return username == serverUsername && password == serverPassword;
} 