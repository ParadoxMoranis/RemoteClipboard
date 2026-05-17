#include "autostartmanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>

AutoStartManager::AutoStartManager(const QString& appName)
    : appName_(appName)
{
}

QString AutoStartManager::commandLine() const
{
    return QStringLiteral("\"%1\" --autostart").arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));
}

bool AutoStartManager::setEnabled(bool enabled, QString* errorMessage) const
{
#ifdef Q_OS_WINDOWS
    QSettings settings(
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
        QSettings::NativeFormat);
    if (enabled) {
        settings.setValue(appName_, commandLine());
    } else {
        settings.remove(appName_);
    }
    if (settings.status() != QSettings::NoError && errorMessage != nullptr) {
        *errorMessage = QStringLiteral("Unable to update Windows startup registry entry.");
    }
    return settings.status() == QSettings::NoError;
#else
    QString base = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/.config");
    }

    const QString autostartDir = QDir(base).filePath(QStringLiteral("autostart"));
    const QString desktopFilePath = QDir(autostartDir).filePath(appName_ + QStringLiteral(".desktop"));

    if (!enabled) {
        QFile::remove(desktopFilePath);
        return true;
    }

    if (!QDir().mkpath(autostartDir)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Unable to create autostart directory: %1").arg(autostartDir);
        }
        return false;
    }

    QFile file(desktopFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Unable to write autostart desktop file: %1").arg(desktopFilePath);
        }
        return false;
    }

    const QString content =
        QStringLiteral("[Desktop Entry]\n")
        + QStringLiteral("Type=Application\n")
        + QStringLiteral("Name=%1\n").arg(appName_)
        + QStringLiteral("Exec=%1\n").arg(commandLine())
        + QStringLiteral("X-GNOME-Autostart-enabled=true\n");
    file.write(content.toUtf8());
    return true;
#endif
}

bool AutoStartManager::isEnabled() const
{
#ifdef Q_OS_WINDOWS
    QSettings settings(
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
        QSettings::NativeFormat);
    return settings.contains(appName_);
#else
    QString base = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/.config");
    }
    const QString desktopFilePath = QDir(base).filePath(QStringLiteral("autostart/%1.desktop").arg(appName_));
    return QFile::exists(desktopFilePath);
#endif
}
