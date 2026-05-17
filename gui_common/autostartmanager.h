#pragma once

#include <QString>

class AutoStartManager
{
public:
    explicit AutoStartManager(const QString& appName);

    bool setEnabled(bool enabled, QString* errorMessage = nullptr) const;
    bool isEnabled() const;

private:
    QString commandLine() const;

    QString appName_;
};
