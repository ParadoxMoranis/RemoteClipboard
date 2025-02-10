#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>

class TcpClient : public QObject
{
    Q_OBJECT
public:
    explicit TcpClient(QObject *parent = nullptr);
    void connectToServer(const QString& host, quint16 port);
    void disconnectFromServer();
    bool isConnected() const;
    void sendData(const QByteArray& data);

signals:
    void connected();
    void disconnected();
    void error(const QString& errorString);
    void authenticationSuccess();
    void authenticationFailed(const QString& reason);
    void dataReceived(const QByteArray& data);

private slots:
    void handleConnected();
    void handleDisconnected();
    void handleError(QAbstractSocket::SocketError socketError);
    void handleReadyRead();

private:
    void processAuthResponse(const QJsonObject& response);
    QTcpSocket* socket;
};

#endif // TCPCLIENT_H 