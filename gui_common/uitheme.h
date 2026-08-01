#pragma once

#include <QString>

class QApplication;

enum class UiColorScheme {
    Light,
    Dark
};

UiColorScheme uiColorSchemeFromName(const QString& name);
QString uiColorSchemeName(UiColorScheme scheme);
void applyUiColorScheme(QApplication& application, UiColorScheme scheme);
