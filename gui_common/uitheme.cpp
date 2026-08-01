#include "uitheme.h"

#include <QApplication>
#include <QColor>
#include <QPalette>

namespace {
QString lightStyleSheet()
{
    return QStringLiteral(R"QSS(
* {
    font-family: "Inter", "Noto Sans", "Segoe UI", sans-serif;
    font-size: 13px;
}
QMainWindow, QDialog {
    background: #F4EFE5;
    color: #111522;
}
QWidget {
    color: #111522;
}
QFrame#heroPanel {
    background: #2457F5;
    border: 3px solid #111522;
}
QLabel#brandEyebrowLabel {
    background: #D6FF3F;
    border: 2px solid #111522;
    color: #111522;
    font-family: "JetBrains Mono", "Noto Sans Mono", monospace;
    font-size: 11px;
    font-weight: 800;
    padding: 4px 8px;
}
QLabel#brandTitleLabel {
    color: #FFFDF7;
    font-size: 28px;
    font-weight: 900;
    letter-spacing: 0;
}
QLabel#brandCaptionLabel {
    color: #DCE5FF;
    font-family: "JetBrains Mono", "Noto Sans Mono", monospace;
    font-size: 11px;
    font-weight: 700;
}
QLabel#connectionStatusLabel {
    background: #FFFDF7;
    border: 2px solid #111522;
    color: #111522;
    font-family: "JetBrains Mono", "Noto Sans Mono", monospace;
    font-size: 11px;
    font-weight: 900;
    padding: 7px 10px;
}
QLabel#connectionStatusLabel[connectionState="online"] {
    background: #D6FF3F;
}
QLabel#connectionStatusLabel[connectionState="pending"] {
    background: #FFD23F;
}
QLabel#connectionStatusLabel[connectionState="error"] {
    background: #FF573D;
    color: #FFFDF7;
}
QGroupBox {
    background: #FFFDF7;
    border: 2px solid #111522;
    border-radius: 0;
    font-weight: 800;
    margin-top: 18px;
    padding-top: 14px;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    background: #D6FF3F;
    border: 2px solid #111522;
    color: #111522;
    font-family: "JetBrains Mono", "Noto Sans Mono", monospace;
    font-size: 11px;
    font-weight: 900;
    left: 12px;
    padding: 4px 10px;
}
QLabel#fieldLabel {
    color: #424958;
    font-family: "JetBrains Mono", "Noto Sans Mono", monospace;
    font-size: 11px;
    font-weight: 800;
}
QLineEdit, QSpinBox, QComboBox, QKeySequenceEdit {
    background: #FFFFFF;
    border: 2px solid #111522;
    border-radius: 0;
    color: #111522;
    min-height: 36px;
    padding: 0 10px;
    selection-background-color: #2457F5;
    selection-color: #FFFFFF;
}
QLineEdit:focus, QSpinBox:focus, QComboBox:focus, QKeySequenceEdit:focus {
    background: #F3F6FF;
    border-color: #2457F5;
}
QLineEdit:disabled, QSpinBox:disabled, QComboBox:disabled {
    background: #E7E2D9;
    color: #777C86;
}
QComboBox::drop-down {
    border: 0;
    border-left: 2px solid #111522;
    width: 32px;
}
QComboBox QAbstractItemView {
    background: #FFFDF7;
    border: 2px solid #111522;
    color: #111522;
    selection-background-color: #D6FF3F;
    selection-color: #111522;
    outline: 0;
}
QSpinBox::up-button, QSpinBox::down-button {
    background: #E9EDFF;
    border: 0;
    border-left: 2px solid #111522;
    width: 28px;
}
QSpinBox::up-button {
    border-bottom: 1px solid #111522;
}
QPushButton {
    background: #D6FF3F;
    border: 2px solid #111522;
    border-radius: 0;
    color: #111522;
    font-weight: 900;
    min-height: 36px;
    padding: 0 14px;
}
QPushButton:hover {
    background: #FF573D;
    color: #FFFDF7;
}
QPushButton:pressed {
    background: #111522;
    color: #FFFDF7;
}
QPushButton:disabled {
    background: #D7D2C9;
    color: #848893;
}
QPushButton#themeButton {
    background: #FFD23F;
}
QPushButton#themeButton:hover {
    background: #D6FF3F;
    color: #111522;
}
QPushButton#btnConnect {
    background: #FF573D;
    border-width: 3px;
    color: #FFFDF7;
    font-size: 14px;
    min-height: 44px;
}
QPushButton#btnConnect:hover {
    background: #2457F5;
}
QPushButton#btnConnect[active="true"] {
    background: #111522;
}
QPushButton[buttonRole="danger"] {
    background: #FF573D;
    color: #FFFDF7;
}
QTextEdit#logTextEdit {
    background: #111522;
    border: 0;
    color: #D6FF3F;
    font-family: "JetBrains Mono", "Noto Sans Mono", monospace;
    font-size: 12px;
    padding: 12px;
    selection-background-color: #2457F5;
    selection-color: #FFFFFF;
}
QListWidget {
    background: #FFFFFF;
    border: 2px solid #111522;
    outline: 0;
    padding: 4px;
}
QListWidget::item {
    border-bottom: 1px solid #C9C5BD;
    min-height: 34px;
    padding: 4px 8px;
}
QListWidget::item:selected {
    background: #2457F5;
    color: #FFFFFF;
}
QCheckBox {
    font-weight: 700;
    spacing: 8px;
}
QCheckBox::indicator {
    background: #FFFFFF;
    border: 2px solid #111522;
    border-radius: 0;
    height: 18px;
    width: 18px;
}
QCheckBox::indicator:checked {
    background: #D6FF3F;
    border: 5px solid #111522;
}
QStatusBar {
    background: #2457F5;
    border-top: 2px solid #111522;
    color: #FFFFFF;
    font-family: "JetBrains Mono", "Noto Sans Mono", monospace;
    font-size: 11px;
    font-weight: 700;
}
QDialogButtonBox {
    background: transparent;
}
QScrollArea#settingsScrollArea, QScrollArea#settingsScrollArea > QWidget > QWidget {
    background: transparent;
    border: 0;
}
QToolTip {
    background: #D6FF3F;
    border: 2px solid #111522;
    color: #111522;
    padding: 6px;
}
QScrollBar:vertical {
    background: #E6E1D8;
    border-left: 2px solid #111522;
    width: 14px;
}
QScrollBar::handle:vertical {
    background: #2457F5;
    min-height: 28px;
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0;
}
)QSS");
}

QString darkStyleSheet()
{
    return QStringLiteral(R"QSS(
* {
    font-family: "Inter", "Noto Sans", "Segoe UI", sans-serif;
    font-size: 13px;
}
QMainWindow, QDialog {
    background: #090D18;
    color: #F4F7FF;
}
QWidget {
    color: #F4F7FF;
}
QFrame#heroPanel {
    background: #131B30;
    border: 3px solid #00D9FF;
}
QLabel#brandEyebrowLabel {
    background: #D6FF3F;
    border: 2px solid #05070D;
    color: #05070D;
    font-family: "JetBrains Mono", "Noto Sans Mono", monospace;
    font-size: 11px;
    font-weight: 800;
    padding: 4px 8px;
}
QLabel#brandTitleLabel {
    color: #F4F7FF;
    font-size: 28px;
    font-weight: 900;
    letter-spacing: 0;
}
QLabel#brandCaptionLabel {
    color: #00D9FF;
    font-family: "JetBrains Mono", "Noto Sans Mono", monospace;
    font-size: 11px;
    font-weight: 700;
}
QLabel#connectionStatusLabel {
    background: #090D18;
    border: 2px solid #00D9FF;
    color: #00D9FF;
    font-family: "JetBrains Mono", "Noto Sans Mono", monospace;
    font-size: 11px;
    font-weight: 900;
    padding: 7px 10px;
}
QLabel#connectionStatusLabel[connectionState="online"] {
    background: #D6FF3F;
    border-color: #D6FF3F;
    color: #05070D;
}
QLabel#connectionStatusLabel[connectionState="pending"] {
    background: #FFD23F;
    border-color: #FFD23F;
    color: #05070D;
}
QLabel#connectionStatusLabel[connectionState="error"] {
    background: #FF3D81;
    border-color: #FF3D81;
    color: #FFFFFF;
}
QGroupBox {
    background: #111827;
    border: 2px solid #00D9FF;
    border-radius: 0;
    font-weight: 800;
    margin-top: 18px;
    padding-top: 14px;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    background: #00D9FF;
    border: 2px solid #05070D;
    color: #05070D;
    font-family: "JetBrains Mono", "Noto Sans Mono", monospace;
    font-size: 11px;
    font-weight: 900;
    left: 12px;
    padding: 4px 10px;
}
QLabel#fieldLabel {
    color: #9EABC2;
    font-family: "JetBrains Mono", "Noto Sans Mono", monospace;
    font-size: 11px;
    font-weight: 800;
}
QLineEdit, QSpinBox, QComboBox, QKeySequenceEdit {
    background: #090D18;
    border: 2px solid #60708F;
    border-radius: 0;
    color: #F4F7FF;
    min-height: 36px;
    padding: 0 10px;
    selection-background-color: #FF3D81;
    selection-color: #FFFFFF;
}
QLineEdit:focus, QSpinBox:focus, QComboBox:focus, QKeySequenceEdit:focus {
    background: #101A2F;
    border-color: #00D9FF;
}
QLineEdit:disabled, QSpinBox:disabled, QComboBox:disabled {
    background: #171D2A;
    color: #6D7890;
}
QComboBox::drop-down {
    border: 0;
    border-left: 2px solid #60708F;
    width: 32px;
}
QComboBox QAbstractItemView {
    background: #111827;
    border: 2px solid #00D9FF;
    color: #F4F7FF;
    selection-background-color: #FF3D81;
    selection-color: #FFFFFF;
    outline: 0;
}
QSpinBox::up-button, QSpinBox::down-button {
    background: #17213A;
    border: 0;
    border-left: 2px solid #60708F;
    width: 28px;
}
QSpinBox::up-button {
    border-bottom: 1px solid #60708F;
}
QPushButton {
    background: #00D9FF;
    border: 2px solid #05070D;
    border-radius: 0;
    color: #05070D;
    font-weight: 900;
    min-height: 36px;
    padding: 0 14px;
}
QPushButton:hover {
    background: #D6FF3F;
    color: #05070D;
}
QPushButton:pressed {
    background: #F4F7FF;
    color: #05070D;
}
QPushButton:disabled {
    background: #333C4E;
    color: #788399;
}
QPushButton#themeButton {
    background: #FFD23F;
}
QPushButton#themeButton:hover {
    background: #FF3D81;
    color: #FFFFFF;
}
QPushButton#btnConnect {
    background: #FF3D81;
    border: 3px solid #F4F7FF;
    color: #FFFFFF;
    font-size: 14px;
    min-height: 44px;
}
QPushButton#btnConnect:hover {
    background: #D6FF3F;
    color: #05070D;
}
QPushButton#btnConnect[active="true"] {
    background: #F4F7FF;
    color: #05070D;
}
QPushButton[buttonRole="danger"] {
    background: #FF3D81;
    color: #FFFFFF;
}
QTextEdit#logTextEdit {
    background: #05070D;
    border: 0;
    color: #00D9FF;
    font-family: "JetBrains Mono", "Noto Sans Mono", monospace;
    font-size: 12px;
    padding: 12px;
    selection-background-color: #FF3D81;
    selection-color: #FFFFFF;
}
QListWidget {
    background: #090D18;
    border: 2px solid #60708F;
    color: #F4F7FF;
    outline: 0;
    padding: 4px;
}
QListWidget::item {
    border-bottom: 1px solid #303B51;
    min-height: 34px;
    padding: 4px 8px;
}
QListWidget::item:selected {
    background: #FF3D81;
    color: #FFFFFF;
}
QCheckBox {
    color: #F4F7FF;
    font-weight: 700;
    spacing: 8px;
}
QCheckBox::indicator {
    background: #090D18;
    border: 2px solid #60708F;
    border-radius: 0;
    height: 18px;
    width: 18px;
}
QCheckBox::indicator:checked {
    background: #D6FF3F;
    border: 5px solid #05070D;
}
QStatusBar {
    background: #00D9FF;
    border-top: 2px solid #05070D;
    color: #05070D;
    font-family: "JetBrains Mono", "Noto Sans Mono", monospace;
    font-size: 11px;
    font-weight: 800;
}
QDialogButtonBox {
    background: transparent;
}
QScrollArea#settingsScrollArea, QScrollArea#settingsScrollArea > QWidget > QWidget {
    background: transparent;
    border: 0;
}
QToolTip {
    background: #D6FF3F;
    border: 2px solid #05070D;
    color: #05070D;
    padding: 6px;
}
QScrollBar:vertical {
    background: #111827;
    border-left: 2px solid #60708F;
    width: 14px;
}
QScrollBar::handle:vertical {
    background: #00D9FF;
    min-height: 28px;
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0;
}
)QSS");
}
}

UiColorScheme uiColorSchemeFromName(const QString& name)
{
    return name.compare(QStringLiteral("dark"), Qt::CaseInsensitive) == 0
        ? UiColorScheme::Dark
        : UiColorScheme::Light;
}

QString uiColorSchemeName(UiColorScheme scheme)
{
    return scheme == UiColorScheme::Dark ? QStringLiteral("dark") : QStringLiteral("light");
}

void applyUiColorScheme(QApplication& application, UiColorScheme scheme)
{
    const bool dark = scheme == UiColorScheme::Dark;
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(dark ? "#090D18" : "#F4EFE5"));
    palette.setColor(QPalette::WindowText, QColor(dark ? "#F4F7FF" : "#111522"));
    palette.setColor(QPalette::Base, QColor(dark ? "#090D18" : "#FFFFFF"));
    palette.setColor(QPalette::AlternateBase, QColor(dark ? "#111827" : "#F4EFE5"));
    palette.setColor(QPalette::Text, QColor(dark ? "#F4F7FF" : "#111522"));
    palette.setColor(QPalette::Button, QColor(dark ? "#00D9FF" : "#D6FF3F"));
    palette.setColor(QPalette::ButtonText, QColor("#05070D"));
    palette.setColor(QPalette::Highlight, QColor(dark ? "#FF3D81" : "#2457F5"));
    palette.setColor(QPalette::HighlightedText, QColor("#FFFFFF"));
    palette.setColor(QPalette::ToolTipBase, QColor("#D6FF3F"));
    palette.setColor(QPalette::ToolTipText, QColor("#05070D"));
    application.setPalette(palette);
    application.setStyleSheet(dark ? darkStyleSheet() : lightStyleSheet());
}
