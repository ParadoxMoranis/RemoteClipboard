#pragma once

#include <QString>

class QApplication;

QString normalizedUiLanguage(const QString& language);
void applyUiLanguage(QApplication& application, const QString& language);
