#pragma once

#include <QDialog>

#include "clientconfig.h"

class QCheckBox;
class QComboBox;
class QKeySequenceEdit;
class QLineEdit;
class QListWidget;
class QSpinBox;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(const GuiAppConfig& config, QWidget* parent = nullptr);

    GuiAppConfig config() const;

private slots:
    void onProfileSelectionChanged();
    void onAddProfile();
    void onRemoveProfile();
    void onBrowseReceiveDirectory();
    void onBrowseCaCertificate();
    void onFieldEdited();

private:
    void populateProfiles();
    void loadProfileToEditor(int index);
    void saveEditorToProfile(int index);
    void ensureProfileSelection();

    GuiAppConfig config_;
    bool populatingEditor_ = false;
    int lastProfileIndex_ = -1;

    QListWidget* profileList_ = nullptr;
    QLineEdit* profileNameEdit_ = nullptr;
    QLineEdit* hostEdit_ = nullptr;
    QSpinBox* portSpinBox_ = nullptr;
    QLineEdit* usernameEdit_ = nullptr;
    QLineEdit* passwordEdit_ = nullptr;
    QCheckBox* useTlsCheckBox_ = nullptr;
    QLineEdit* caCertificateEdit_ = nullptr;
    QLineEdit* receiveDirectoryEdit_ = nullptr;
    QComboBox* languageComboBox_ = nullptr;
    QCheckBox* autoStartCheckBox_ = nullptr;
    QCheckBox* autoConnectCheckBox_ = nullptr;
    QKeySequenceEdit* showWindowShortcutEdit_ = nullptr;
    QKeySequenceEdit* switchProfileShortcutEdit_ = nullptr;
};
