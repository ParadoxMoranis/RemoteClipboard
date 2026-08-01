#include "settingsdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(const GuiAppConfig& config, QWidget* parent)
    : QDialog(parent)
    , config_(config)
{
    setObjectName(QStringLiteral("settingsDialog"));
    setWindowTitle(tr("Remote Clipboard // Settings"));
    setMinimumSize(620, 560);
    resize(660, 700);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(18, 18, 18, 18);
    rootLayout->setSpacing(14);

    auto* dialogHero = new QFrame(this);
    dialogHero->setObjectName(QStringLiteral("heroPanel"));
    auto* dialogHeroLayout = new QVBoxLayout(dialogHero);
    dialogHeroLayout->setContentsMargins(18, 14, 18, 14);
    dialogHeroLayout->setSpacing(5);
    auto* dialogEyebrow = new QLabel(tr("CONFIGURATION // WORKSPACE"), dialogHero);
    dialogEyebrow->setObjectName(QStringLiteral("brandEyebrowLabel"));
    dialogEyebrow->setAlignment(Qt::AlignLeft);
    auto* dialogTitle = new QLabel(tr("CLIENT / SETTINGS"), dialogHero);
    dialogTitle->setObjectName(QStringLiteral("brandTitleLabel"));
    auto* dialogCaption = new QLabel(tr("PROFILES · STARTUP · SHORTCUTS · TRANSPORT SECURITY"), dialogHero);
    dialogCaption->setObjectName(QStringLiteral("brandCaptionLabel"));
    dialogHeroLayout->addWidget(dialogEyebrow, 0, Qt::AlignLeft);
    dialogHeroLayout->addWidget(dialogTitle);
    dialogHeroLayout->addWidget(dialogCaption);
    rootLayout->addWidget(dialogHero);

    auto* contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(14);

    auto* profileGroup = new QGroupBox(tr("SERVER PROFILES // LIST"), this);
    profileGroup->setMinimumWidth(190);
    auto* profileLayout = new QVBoxLayout(profileGroup);
    profileList_ = new QListWidget(profileGroup);
    auto* profileButtonLayout = new QHBoxLayout();
    auto* addProfileButton = new QPushButton(tr("Add"), profileGroup);
    auto* removeProfileButton = new QPushButton(tr("Remove"), profileGroup);
    addProfileButton->setText(tr("ADD"));
    removeProfileButton->setText(tr("REMOVE"));
    removeProfileButton->setProperty("buttonRole", QStringLiteral("danger"));
    profileButtonLayout->addWidget(addProfileButton);
    profileButtonLayout->addWidget(removeProfileButton);
    profileLayout->addWidget(profileList_);
    profileLayout->addLayout(profileButtonLayout);

    auto* editorContainer = new QWidget(this);
    auto* editorLayout = new QVBoxLayout(editorContainer);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(14);

    auto* connectionGroup = new QGroupBox(tr("PROFILE // DETAILS"), editorContainer);
    auto* connectionForm = new QFormLayout(connectionGroup);
    connectionForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    connectionForm->setRowWrapPolicy(QFormLayout::WrapLongRows);
    profileNameEdit_ = new QLineEdit(connectionGroup);
    hostEdit_ = new QLineEdit(connectionGroup);
    portSpinBox_ = new QSpinBox(connectionGroup);
    portSpinBox_->setRange(1024, 65535);
    usernameEdit_ = new QLineEdit(connectionGroup);
    passwordEdit_ = new QLineEdit(connectionGroup);
    passwordEdit_->setEchoMode(QLineEdit::Password);
    useTlsCheckBox_ = new QCheckBox(tr("Enable TLS"), connectionGroup);
    caCertificateEdit_ = new QLineEdit(connectionGroup);
    auto* caBrowseButton = new QPushButton(tr("Browse"), connectionGroup);
    auto* caLayout = new QHBoxLayout();
    caLayout->setContentsMargins(0, 0, 0, 0);
    caLayout->setSpacing(8);
    caLayout->addWidget(caCertificateEdit_);
    caLayout->addWidget(caBrowseButton);
    auto* caContainer = new QWidget(connectionGroup);
    caContainer->setLayout(caLayout);

    connectionForm->addRow(tr("Profile Name"), profileNameEdit_);
    connectionForm->addRow(tr("Server Host"), hostEdit_);
    connectionForm->addRow(tr("Port"), portSpinBox_);
    connectionForm->addRow(tr("Username"), usernameEdit_);
    connectionForm->addRow(tr("Password"), passwordEdit_);
    connectionForm->addRow(QString(), useTlsCheckBox_);
    connectionForm->addRow(tr("CA Certificate"), caContainer);

    auto* appGroup = new QGroupBox(tr("CLIENT // BEHAVIOR"), editorContainer);
    auto* appForm = new QFormLayout(appGroup);
    appForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    appForm->setRowWrapPolicy(QFormLayout::WrapLongRows);
    receiveDirectoryEdit_ = new QLineEdit(appGroup);
    auto* receiveBrowseButton = new QPushButton(tr("Browse"), appGroup);
    auto* receiveLayout = new QHBoxLayout();
    receiveLayout->setContentsMargins(0, 0, 0, 0);
    receiveLayout->setSpacing(8);
    receiveLayout->addWidget(receiveDirectoryEdit_);
    receiveLayout->addWidget(receiveBrowseButton);
    auto* receiveContainer = new QWidget(appGroup);
    receiveContainer->setLayout(receiveLayout);
    languageComboBox_ = new QComboBox(appGroup);
    languageComboBox_->addItem(QStringLiteral("English"), QStringLiteral("en"));
    languageComboBox_->addItem(QStringLiteral("简体中文"), QStringLiteral("zh_CN"));
    autoStartCheckBox_ = new QCheckBox(tr("Launch on system startup"), appGroup);
    autoConnectCheckBox_ = new QCheckBox(tr("Auto-connect last profile on startup"), appGroup);
    showWindowShortcutEdit_ = new QKeySequenceEdit(appGroup);
    switchProfileShortcutEdit_ = new QKeySequenceEdit(appGroup);

    appForm->addRow(tr("Language"), languageComboBox_);
    appForm->addRow(tr("Receive Directory"), receiveContainer);
    appForm->addRow(QString(), autoStartCheckBox_);
    appForm->addRow(QString(), autoConnectCheckBox_);
    appForm->addRow(tr("Show Window Hotkey"), showWindowShortcutEdit_);
    appForm->addRow(tr("Switch Profile Hotkey"), switchProfileShortcutEdit_);

    editorLayout->addWidget(connectionGroup);
    editorLayout->addWidget(appGroup);
    editorLayout->addStretch(1);

    auto* editorScrollArea = new QScrollArea(this);
    editorScrollArea->setObjectName(QStringLiteral("settingsScrollArea"));
    editorScrollArea->setWidgetResizable(true);
    editorScrollArea->setFrameShape(QFrame::NoFrame);
    editorScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    editorScrollArea->setWidget(editorContainer);

    contentLayout->addWidget(profileGroup, 1);
    contentLayout->addWidget(editorScrollArea, 2);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Ok)->setText(tr("SAVE // APPLY"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("CANCEL"));
    buttonBox->button(QDialogButtonBox::Ok)->setIcon(QIcon());
    buttonBox->button(QDialogButtonBox::Cancel)->setIcon(QIcon());
    rootLayout->addLayout(contentLayout);
    rootLayout->addWidget(buttonBox);

    receiveDirectoryEdit_->setText(config_.receiveDirectory);
    const int languageIndex = languageComboBox_->findData(config_.language);
    languageComboBox_->setCurrentIndex(languageIndex >= 0 ? languageIndex : 0);
    autoStartCheckBox_->setChecked(config_.autoStartEnabled);
    autoConnectCheckBox_->setChecked(config_.autoConnectLastProfile);
    showWindowShortcutEdit_->setKeySequence(config_.showWindowShortcut);
    switchProfileShortcutEdit_->setKeySequence(config_.switchProfileShortcut);

    populateProfiles();
    ensureProfileSelection();

    connect(profileList_, &QListWidget::currentRowChanged, this, &SettingsDialog::onProfileSelectionChanged);
    connect(addProfileButton, &QPushButton::clicked, this, &SettingsDialog::onAddProfile);
    connect(removeProfileButton, &QPushButton::clicked, this, &SettingsDialog::onRemoveProfile);
    connect(receiveBrowseButton, &QPushButton::clicked, this, &SettingsDialog::onBrowseReceiveDirectory);
    connect(caBrowseButton, &QPushButton::clicked, this, &SettingsDialog::onBrowseCaCertificate);
    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        saveEditorToProfile(profileList_->currentRow());
        config_.receiveDirectory = receiveDirectoryEdit_->text().trimmed();
        config_.language = languageComboBox_->currentData().toString();
        config_.autoStartEnabled = autoStartCheckBox_->isChecked();
        config_.autoConnectLastProfile = autoConnectCheckBox_->isChecked();
        config_.showWindowShortcut = showWindowShortcutEdit_->keySequence();
        config_.switchProfileShortcut = switchProfileShortcutEdit_->keySequence();

        if (config_.profiles.isEmpty()) {
            QMessageBox::warning(this, tr("Missing Profile"), tr("Please keep at least one server profile."));
            return;
        }
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);

    const auto connectField = [this](QObject* object) {
        connect(object, SIGNAL(editingFinished()), this, SLOT(onFieldEdited()));
    };
    connectField(profileNameEdit_);
    connectField(hostEdit_);
    connectField(usernameEdit_);
    connectField(passwordEdit_);
    connectField(caCertificateEdit_);
    connect(portSpinBox_, &QSpinBox::valueChanged, this, &SettingsDialog::onFieldEdited);
    connect(useTlsCheckBox_, &QCheckBox::toggled, this, &SettingsDialog::onFieldEdited);
}

GuiAppConfig SettingsDialog::config() const
{
    return config_;
}

void SettingsDialog::populateProfiles()
{
    profileList_->clear();
    for (const ConnectionProfile& profile : config_.profiles) {
        profileList_->addItem(profile.name);
    }
}

void SettingsDialog::ensureProfileSelection()
{
    if (config_.profiles.isEmpty()) {
        ConnectionProfile profile = GuiConfigStore::defaultProfile();
        config_.profiles.append(profile);
        config_.selectedProfileId = profile.id;
        populateProfiles();
    }

    int selectedIndex = 0;
    for (int index = 0; index < config_.profiles.size(); ++index) {
        if (config_.profiles[index].id == config_.selectedProfileId) {
            selectedIndex = index;
            break;
        }
    }

    profileList_->setCurrentRow(selectedIndex);
    lastProfileIndex_ = selectedIndex;
    loadProfileToEditor(selectedIndex);
}

void SettingsDialog::loadProfileToEditor(int index)
{
    if (index < 0 || index >= config_.profiles.size()) {
        return;
    }

    populatingEditor_ = true;
    const ConnectionProfile& profile = config_.profiles[index];
    profileNameEdit_->setText(profile.name);
    hostEdit_->setText(profile.host);
    portSpinBox_->setValue(profile.port);
    usernameEdit_->setText(profile.username);
    passwordEdit_->setText(profile.password);
    useTlsCheckBox_->setChecked(profile.useTls);
    caCertificateEdit_->setText(profile.caCertificatePath);
    populatingEditor_ = false;
}

void SettingsDialog::saveEditorToProfile(int index)
{
    if (populatingEditor_ || index < 0 || index >= config_.profiles.size()) {
        return;
    }

    ConnectionProfile& profile = config_.profiles[index];
    profile.name = profileNameEdit_->text().trimmed();
    profile.host = hostEdit_->text().trimmed();
    profile.port = static_cast<quint16>(portSpinBox_->value());
    profile.username = usernameEdit_->text().trimmed();
    profile.password = passwordEdit_->text();
    profile.useTls = useTlsCheckBox_->isChecked();
    profile.caCertificatePath = caCertificateEdit_->text().trimmed();

    if (profile.name.isEmpty()) {
        profile.name = profile.host.isEmpty() ? tr("Profile %1").arg(index + 1) : profile.host;
    }

    profileList_->item(index)->setText(profile.name);
    config_.selectedProfileId = profile.id;
}

void SettingsDialog::onProfileSelectionChanged()
{
    saveEditorToProfile(lastProfileIndex_);
    lastProfileIndex_ = profileList_->currentRow();
    loadProfileToEditor(lastProfileIndex_);
}

void SettingsDialog::onAddProfile()
{
    saveEditorToProfile(profileList_->currentRow());

    ConnectionProfile profile = GuiConfigStore::defaultProfile();
    profile.name = tr("Profile %1").arg(config_.profiles.size() + 1);
    config_.profiles.append(profile);
    populateProfiles();
    profileList_->setCurrentRow(config_.profiles.size() - 1);
}

void SettingsDialog::onRemoveProfile()
{
    const int index = profileList_->currentRow();
    if (index < 0 || index >= config_.profiles.size()) {
        return;
    }

    if (config_.profiles.size() == 1) {
        QMessageBox::warning(this, tr("Cannot Remove"), tr("At least one profile must remain."));
        return;
    }

    config_.profiles.removeAt(index);
    populateProfiles();
    profileList_->setCurrentRow(qMin(index, config_.profiles.size() - 1));
}

void SettingsDialog::onBrowseReceiveDirectory()
{
    const QString directory = QFileDialog::getExistingDirectory(
        this,
        tr("Select Receive Directory"),
        receiveDirectoryEdit_->text());
    if (!directory.isEmpty()) {
        receiveDirectoryEdit_->setText(directory);
    }
}

void SettingsDialog::onBrowseCaCertificate()
{
    const QString file = QFileDialog::getOpenFileName(
        this,
        tr("Select CA Certificate"),
        caCertificateEdit_->text(),
        tr("Certificate Files (*.pem *.crt *.cer);;All Files (*)"));
    if (!file.isEmpty()) {
        caCertificateEdit_->setText(file);
        onFieldEdited();
    }
}

void SettingsDialog::onFieldEdited()
{
    saveEditorToProfile(profileList_->currentRow());
}
