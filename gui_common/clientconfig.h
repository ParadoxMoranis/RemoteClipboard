#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QKeySequence>
#include <QString>
#include <QVector>

struct ConnectionProfile {
    QString id;
    QString name;
    QString host;
    quint16 port = 8080;
    QString username;
    QString password;
    bool useTls = false;
    QString caCertificatePath;
    bool allowInsecureTls = true;
};

struct GuiAppConfig {
    QVector<ConnectionProfile> profiles;
    QString selectedProfileId;
    QString lastConnectedProfileId;
    QString receiveDirectory;
    bool autoStartEnabled = false;
    bool autoConnectLastProfile = true;
    QKeySequence showWindowShortcut;
    QKeySequence switchProfileShortcut;
};

class GuiConfigStore
{
public:
    explicit GuiConfigStore(const QString& applicationName);

    QString configPath() const;
    GuiAppConfig load() const;
    bool save(const GuiAppConfig& config, QString* errorMessage = nullptr) const;

    static ConnectionProfile defaultProfile();
    static QString generateProfileId();
    static ConnectionProfile profileFromJson(const QJsonObject& object);
    static QJsonObject profileToJson(const ConnectionProfile& profile);
    static ConnectionProfile* findProfileById(GuiAppConfig& config, const QString& profileId);
    static const ConnectionProfile* findProfileById(const GuiAppConfig& config, const QString& profileId);

private:
    GuiAppConfig createDefaultConfig() const;
    QString configDirectory() const;

    QString applicationName_;
};
