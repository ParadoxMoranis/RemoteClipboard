#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include <QObject>
#include <QJsonObject>
#include <QSslSocket>
#include <QTimer>

class TcpClient : public QObject
{
    Q_OBJECT
public:
    explicit TcpClient(QObject *parent = nullptr);

    void connectToServer(const QString& host,
                         quint16 port,
                         bool useTls,
                         const QString& caCertificatePath = QString(),
                         bool allowInsecureTls = true);
    void disconnectFromServer();

    bool isConnected() const;
    bool isAuthenticated() const;
    void sendJson(const QJsonObject& object);

signals:
    void connected();
    void disconnected();
    void error(const QString& errorString);
    void authResponse(const QJsonObject& response);
    void dataReceived(const QJsonObject& data);
    void reconnectScheduled(int attempt, int delayMs);

private slots:
    void handleConnected();
    void handleEncrypted();
    void handleDisconnected();
    void handleError(QAbstractSocket::SocketError socketError);
    void handleReadyRead();
    void handleSslErrors(const QList<QSslError>& errors);
    void attemptReconnect();
    void sendHeartbeat();
    void handleHeartbeatTimeout();

private:
    void beginConnection();
    void scheduleReconnect();
    void stopReconnect();
    void stopHeartbeat();
    void resetHeartbeat();

    QSslSocket* socket;
    QByteArray readBuffer;
    QString host;
    quint16 port = 0;
    bool useTls = false;
    QString caCertificatePath;
    bool allowInsecureTls = true;
    bool manualDisconnect = false;
    bool authenticated = false;
    int reconnectAttempt = 0;

    QTimer reconnectTimer;
    QTimer heartbeatTimer;
    QTimer heartbeatTimeoutTimer;
};

#endif // TCPCLIENT_H
