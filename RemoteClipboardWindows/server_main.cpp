#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include "tcpserver.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    QCoreApplication::setApplicationName("RemoteClipboardServer");
    QCoreApplication::setApplicationVersion("1.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("Remote Clipboard Server");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption portOption(QStringList() << "p" << "port",
            "Port to listen on.", "port", "8080");
    QCommandLineOption usernameOption(QStringList() << "u" << "username",
            "Username for authentication.", "username", "admin");
    QCommandLineOption passwordOption(QStringList() << "w" << "password",
            "Password for authentication.", "password", "admin");

    parser.addOption(portOption);
    parser.addOption(usernameOption);
    parser.addOption(passwordOption);

    parser.process(a);

    quint16 port = parser.value(portOption).toUShort();
    QString username = parser.value(usernameOption);
    QString password = parser.value(passwordOption);

    TcpServer server;
    server.setCredentials(username, password);
    if (!server.startServer(port)) {
        qDebug() << "Failed to start server on port" << port;
        return 1;
    }

    qDebug() << "Server started on port" << port;
    qDebug() << "Username:" << username;
    qDebug() << "Password:" << password;

    return a.exec();
} 