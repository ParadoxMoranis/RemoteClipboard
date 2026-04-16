#include "tcpclient.h"

#include <QJsonDocument>
#include <QSslCertificate>
#include <QSslConfiguration>

namespace {
constexpr int kHeartbeatIntervalMs = 10000;
constexpr int kHeartbeatTimeoutMs = 30000;
constexpr int kReconnectBaseDelayMs = 1000;
constexpr int kReconnectMaxDelayMs = 30000;
}

TcpClient::TcpClient(QObject *parent)
    : QObject(parent)
    , socket(new QSslSocket(this))
{
    connect(socket, &QSslSocket::connected, this, &TcpClient::handleConnected);
    connect(socket, &QSslSocket::encrypted, this, &TcpClient::handleEncrypted);
    connect(socket, &QSslSocket::disconnected, this, &TcpClient::handleDisconnected);
    connect(socket, &QSslSocket::errorOccurred, this, &TcpClient::handleError);
    connect(socket, &QSslSocket::readyRead, this, &TcpClient::handleReadyRead);
    connect(socket, &QSslSocket::sslErrors, this, &TcpClient::handleSslErrors);

    reconnectTimer.setSingleShot(true);
    connect(&reconnectTimer, &QTimer::timeout, this, &TcpClient::attemptReconnect);

    heartbeatTimer.setInterval(kHeartbeatIntervalMs);
    connect(&heartbeatTimer, &QTimer::timeout, this, &TcpClient::sendHeartbeat);

    heartbeatTimeoutTimer.setSingleShot(true);
    heartbeatTimeoutTimer.setInterval(kHeartbeatTimeoutMs);
    connect(&heartbeatTimeoutTimer, &QTimer::timeout, this, &TcpClient::handleHeartbeatTimeout);
}

void TcpClient::connectToServer(const QString& host,
                                quint16 port,
                                bool useTls,
                                const QString& caCertificatePath,
                                bool allowInsecureTls)
{
    this->host = host;
    this->port = port;
    this->useTls = useTls;
    this->caCertificatePath = caCertificatePath;
    this->allowInsecureTls = allowInsecureTls;
    manualDisconnect = false;
    reconnectAttempt = 0;

    beginConnection();
}

void TcpClient::disconnectFromServer()
{
    manualDisconnect = true;
    stopReconnect();
    stopHeartbeat();
    authenticated = false;
    readBuffer.clear();
    socket->disconnectFromHost();
}

bool TcpClient::isConnected() const
{
    if (socket->state() != QAbstractSocket::ConnectedState) {
        return false;
    }
    return !useTls || socket->isEncrypted();
}

bool TcpClient::isAuthenticated() const
{
    return authenticated;
}

void TcpClient::sendJson(const QJsonObject& object)
{
    if (!isConnected()) {
        return;
    }

    const QByteArray payload = QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
    socket->write(payload);
    socket->flush();
}

void TcpClient::handleConnected()
{
    if (!useTls) {
        reconnectAttempt = 0;
        emit connected();
    }
}

void TcpClient::handleEncrypted()
{
    reconnectAttempt = 0;
    emit connected();
}

void TcpClient::handleDisconnected()
{
    stopHeartbeat();
    authenticated = false;
    emit disconnected();
    scheduleReconnect();
}

void TcpClient::handleError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    emit error(socket->errorString());
    if (socket->state() == QAbstractSocket::UnconnectedState) {
        scheduleReconnect();
    }
}

void TcpClient::handleReadyRead()
{
    readBuffer.append(socket->readAll());

    int newlineIndex = -1;
    while ((newlineIndex = readBuffer.indexOf('\n')) >= 0) {
        const QByteArray line = readBuffer.left(newlineIndex).trimmed();
        readBuffer.remove(0, newlineIndex + 1);

        if (line.isEmpty()) {
            continue;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            emit error(QStringLiteral("Invalid message from server: %1").arg(parseError.errorString()));
            continue;
        }

        const QJsonObject object = document.object();
        const QString type = object.value("type").toString();

        if (type == "auth_response") {
            authenticated = object.value("status").toString() == "ok";
            if (authenticated) {
                resetHeartbeat();
            } else {
                stopHeartbeat();
            }
            emit authResponse(object);
            continue;
        }

        if (type == "pong") {
            heartbeatTimeoutTimer.stop();
            continue;
        }

        emit dataReceived(object);
    }
}

void TcpClient::handleSslErrors(const QList<QSslError>& errors)
{
    if (allowInsecureTls) {
        socket->ignoreSslErrors(errors);
        return;
    }

    if (!errors.isEmpty()) {
        emit error(errors.first().errorString());
    }
}

void TcpClient::attemptReconnect()
{
    if (manualDisconnect) {
        return;
    }
    beginConnection();
}

void TcpClient::sendHeartbeat()
{
    if (!authenticated || !isConnected()) {
        return;
    }

    sendJson(QJsonObject{{"type", "ping"}});
    heartbeatTimeoutTimer.start();
}

void TcpClient::handleHeartbeatTimeout()
{
    emit error(QStringLiteral("Heartbeat timeout, reconnecting"));
    socket->abort();
    scheduleReconnect();
}

void TcpClient::beginConnection()
{
    stopReconnect();
    stopHeartbeat();
    authenticated = false;
    readBuffer.clear();

    if (socket->state() != QAbstractSocket::UnconnectedState) {
        socket->abort();
    }

    if (useTls) {
        QSslConfiguration configuration = socket->sslConfiguration();
        configuration.setProtocol(QSsl::TlsV1_2OrLater);

        if (!caCertificatePath.isEmpty()) {
            const QList<QSslCertificate> certificates = QSslCertificate::fromPath(caCertificatePath);
            if (!certificates.isEmpty()) {
                configuration.setCaCertificates(certificates);
            }
        }

        socket->setSslConfiguration(configuration);
        socket->setPeerVerifyMode(allowInsecureTls ? QSslSocket::VerifyNone : QSslSocket::AutoVerifyPeer);
        socket->connectToHostEncrypted(host, port);
        return;
    }

    socket->connectToHost(host, port);
}

void TcpClient::scheduleReconnect()
{
    if (manualDisconnect || reconnectTimer.isActive()) {
        return;
    }

    const int cappedAttempt = qMin(reconnectAttempt, 5);
    const int delayMs = qMin(kReconnectBaseDelayMs * (1 << cappedAttempt), kReconnectMaxDelayMs);
    ++reconnectAttempt;
    reconnectTimer.start(delayMs);
    emit reconnectScheduled(reconnectAttempt, delayMs);
}

void TcpClient::stopReconnect()
{
    reconnectTimer.stop();
}

void TcpClient::stopHeartbeat()
{
    heartbeatTimer.stop();
    heartbeatTimeoutTimer.stop();
}

void TcpClient::resetHeartbeat()
{
    heartbeatTimer.start();
    heartbeatTimeoutTimer.stop();
}
