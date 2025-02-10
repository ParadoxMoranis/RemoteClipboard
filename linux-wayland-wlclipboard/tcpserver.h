#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>

class TcpServer : public QObject
{
    Q_OBJECT
public:
    explicit TcpServer(QObject *parent = nullptr);
    bool startServer(quint16 port);
    void stopServer();
    void setCredentials(const QString &username, const QString &password);

signals:
    void clientConnected(const QString &clientId);
    void clientDisconnected(const QString &clientId);
    void dataReceived(const QString &clientId, const QString &data);
    void errorOccurred(const QString &error);

private slots:
    void handleNewConnection();
    void handleClientDisconnected();
    void handleReadyRead();

private:
    QTcpServer *server;
    QMap<QTcpSocket*, QString> clients;
    QString serverUsername;
    QString serverPassword;

    void broadcastToOthers(QTcpSocket *sender, const QString &data);
    bool validateCredentials(const QString &username, const QString &password);
};

#endif // TCPSERVER_H 