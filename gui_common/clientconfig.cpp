#include "clientconfig.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QUuid>

namespace {
constexpr int kConfigVersion = 1;
constexpr auto kOrgName = "RemoteClipboard";
}

GuiConfigStore::GuiConfigStore(const QString& applicationName)
    : applicationName_(applicationName)
{
}

QString GuiConfigStore::configDirectory() const
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/.config/RemoteClipboard");
    }
    return QDir(base).filePath(applicationName_);
}

QString GuiConfigStore::configPath() const
{
    return QDir(configDirectory()).filePath(QStringLiteral("config.json"));
}

GuiAppConfig GuiConfigStore::createDefaultConfig() const
{
    GuiAppConfig config;
    ConnectionProfile profile = defaultProfile();
    profile.name = QStringLiteral("Default");
    config.profiles.append(profile);
    config.selectedProfileId = profile.id;
    config.showWindowShortcut = QKeySequence(QStringLiteral("Ctrl+Alt+V"));
    config.switchProfileShortcut = QKeySequence(QStringLiteral("Ctrl+Alt+N"));
    return config;
}

GuiAppConfig GuiConfigStore::load() const
{
    QFile file(configPath());
    if (!file.exists()) {
        return createDefaultConfig();
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return createDefaultConfig();
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return createDefaultConfig();
    }

    const QJsonObject root = document.object();
    GuiAppConfig config;
    const QJsonArray profiles = root.value(QStringLiteral("profiles")).toArray();
    for (const QJsonValue& value : profiles) {
        if (value.isObject()) {
            config.profiles.append(profileFromJson(value.toObject()));
        }
    }

    if (config.profiles.isEmpty()) {
        config = createDefaultConfig();
    }

    config.selectedProfileId = root.value(QStringLiteral("selected_profile_id")).toString();
    if (config.selectedProfileId.isEmpty()) {
        config.selectedProfileId = config.profiles.first().id;
    }

    config.lastConnectedProfileId = root.value(QStringLiteral("last_connected_profile_id")).toString();
    config.receiveDirectory = root.value(QStringLiteral("receive_directory")).toString();
    config.autoStartEnabled = root.value(QStringLiteral("autostart_enabled")).toBool(false);
    config.autoConnectLastProfile = root.value(QStringLiteral("autoconnect_last_profile")).toBool(true);
    config.showWindowShortcut = QKeySequence(root.value(QStringLiteral("show_window_shortcut"))
        .toString(QStringLiteral("Ctrl+Alt+V")));
    config.switchProfileShortcut = QKeySequence(root.value(QStringLiteral("switch_profile_shortcut"))
        .toString(QStringLiteral("Ctrl+Alt+N")));

    for (ConnectionProfile& profile : config.profiles) {
        if (profile.id.isEmpty()) {
            profile.id = generateProfileId();
        }
        if (profile.name.isEmpty()) {
            profile.name = profile.host.isEmpty() ? QStringLiteral("Profile") : profile.host;
        }
    }

    if (findProfileById(config, config.selectedProfileId) == nullptr) {
        config.selectedProfileId = config.profiles.first().id;
    }

    return config;
}

bool GuiConfigStore::save(const GuiAppConfig& config, QString* errorMessage) const
{
    QDir dir(configDirectory());
    if (!dir.mkpath(QStringLiteral("."))) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Unable to create config directory: %1").arg(dir.path());
        }
        return false;
    }

    QJsonArray profiles;
    for (const ConnectionProfile& profile : config.profiles) {
        profiles.append(profileToJson(profile));
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), kConfigVersion);
    root.insert(QStringLiteral("organization"), kOrgName);
    root.insert(QStringLiteral("profiles"), profiles);
    root.insert(QStringLiteral("selected_profile_id"), config.selectedProfileId);
    root.insert(QStringLiteral("last_connected_profile_id"), config.lastConnectedProfileId);
    root.insert(QStringLiteral("receive_directory"), config.receiveDirectory);
    root.insert(QStringLiteral("autostart_enabled"), config.autoStartEnabled);
    root.insert(QStringLiteral("autoconnect_last_profile"), config.autoConnectLastProfile);
    root.insert(QStringLiteral("show_window_shortcut"), config.showWindowShortcut.toString(QKeySequence::PortableText));
    root.insert(QStringLiteral("switch_profile_shortcut"), config.switchProfileShortcut.toString(QKeySequence::PortableText));

    QFile file(configPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Unable to write config file: %1").arg(file.fileName());
        }
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

ConnectionProfile GuiConfigStore::defaultProfile()
{
    ConnectionProfile profile;
    profile.id = generateProfileId();
    profile.name = QStringLiteral("Default");
    profile.host = QStringLiteral("127.0.0.1");
    profile.port = 8080;
    profile.username = QStringLiteral("admin");
    profile.password = QStringLiteral("admin");
    profile.useTls = false;
    profile.allowInsecureTls = true;
    return profile;
}

QString GuiConfigStore::generateProfileId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

ConnectionProfile GuiConfigStore::profileFromJson(const QJsonObject& object)
{
    ConnectionProfile profile;
    profile.id = object.value(QStringLiteral("id")).toString();
    profile.name = object.value(QStringLiteral("name")).toString();
    profile.host = object.value(QStringLiteral("host")).toString(QStringLiteral("127.0.0.1"));
    profile.port = static_cast<quint16>(object.value(QStringLiteral("port")).toInt(8080));
    profile.username = object.value(QStringLiteral("username")).toString(QStringLiteral("admin"));
    profile.password = object.value(QStringLiteral("password")).toString(QStringLiteral("admin"));
    profile.useTls = object.value(QStringLiteral("use_tls")).toBool(false);
    profile.caCertificatePath = object.value(QStringLiteral("ca_certificate")).toString();
    profile.allowInsecureTls = object.value(QStringLiteral("allow_insecure_tls")).toBool(true);
    return profile;
}

QJsonObject GuiConfigStore::profileToJson(const ConnectionProfile& profile)
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), profile.id);
    object.insert(QStringLiteral("name"), profile.name);
    object.insert(QStringLiteral("host"), profile.host);
    object.insert(QStringLiteral("port"), static_cast<int>(profile.port));
    object.insert(QStringLiteral("username"), profile.username);
    object.insert(QStringLiteral("password"), profile.password);
    object.insert(QStringLiteral("use_tls"), profile.useTls);
    object.insert(QStringLiteral("ca_certificate"), profile.caCertificatePath);
    object.insert(QStringLiteral("allow_insecure_tls"), profile.allowInsecureTls);
    return object;
}

ConnectionProfile* GuiConfigStore::findProfileById(GuiAppConfig& config, const QString& profileId)
{
    for (ConnectionProfile& profile : config.profiles) {
        if (profile.id == profileId) {
            return &profile;
        }
    }
    return nullptr;
}

const ConnectionProfile* GuiConfigStore::findProfileById(const GuiAppConfig& config, const QString& profileId)
{
    for (const ConnectionProfile& profile : config.profiles) {
        if (profile.id == profileId) {
            return &profile;
        }
    }
    return nullptr;
}
