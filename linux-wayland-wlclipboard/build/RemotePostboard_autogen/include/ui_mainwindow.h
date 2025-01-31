/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.8.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QVBoxLayout *verticalLayout;
    QGroupBox *clientGroup;
    QGridLayout *gridLayout;
    QLabel *serverLabel;
    QLineEdit *serverAddressEdit;
    QLabel *portLabel;
    QSpinBox *portSpinBox;
    QLabel *usernameLabel;
    QLineEdit *usernameEdit;
    QLabel *passwordLabel;
    QLineEdit *passwordEdit;
    QPushButton *btnConnect;
    QTextEdit *logTextEdit;
    QStatusBar *statusBar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(400, 400);
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");
        verticalLayout = new QVBoxLayout(centralwidget);
        verticalLayout->setObjectName("verticalLayout");
        clientGroup = new QGroupBox(centralwidget);
        clientGroup->setObjectName("clientGroup");
        gridLayout = new QGridLayout(clientGroup);
        gridLayout->setObjectName("gridLayout");
        serverLabel = new QLabel(clientGroup);
        serverLabel->setObjectName("serverLabel");

        gridLayout->addWidget(serverLabel, 0, 0, 1, 1);

        serverAddressEdit = new QLineEdit(clientGroup);
        serverAddressEdit->setObjectName("serverAddressEdit");

        gridLayout->addWidget(serverAddressEdit, 0, 1, 1, 1);

        portLabel = new QLabel(clientGroup);
        portLabel->setObjectName("portLabel");

        gridLayout->addWidget(portLabel, 1, 0, 1, 1);

        portSpinBox = new QSpinBox(clientGroup);
        portSpinBox->setObjectName("portSpinBox");
        portSpinBox->setMinimum(1024);
        portSpinBox->setMaximum(65535);
        portSpinBox->setValue(8080);

        gridLayout->addWidget(portSpinBox, 1, 1, 1, 1);

        usernameLabel = new QLabel(clientGroup);
        usernameLabel->setObjectName("usernameLabel");

        gridLayout->addWidget(usernameLabel, 2, 0, 1, 1);

        usernameEdit = new QLineEdit(clientGroup);
        usernameEdit->setObjectName("usernameEdit");

        gridLayout->addWidget(usernameEdit, 2, 1, 1, 1);

        passwordLabel = new QLabel(clientGroup);
        passwordLabel->setObjectName("passwordLabel");

        gridLayout->addWidget(passwordLabel, 3, 0, 1, 1);

        passwordEdit = new QLineEdit(clientGroup);
        passwordEdit->setObjectName("passwordEdit");
        passwordEdit->setEchoMode(QLineEdit::Password);

        gridLayout->addWidget(passwordEdit, 3, 1, 1, 1);

        btnConnect = new QPushButton(clientGroup);
        btnConnect->setObjectName("btnConnect");

        gridLayout->addWidget(btnConnect, 4, 0, 1, 2);


        verticalLayout->addWidget(clientGroup);

        logTextEdit = new QTextEdit(centralwidget);
        logTextEdit->setObjectName("logTextEdit");
        logTextEdit->setReadOnly(true);

        verticalLayout->addWidget(logTextEdit);

        MainWindow->setCentralWidget(centralwidget);
        statusBar = new QStatusBar(MainWindow);
        statusBar->setObjectName("statusBar");
        MainWindow->setStatusBar(statusBar);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "Remote Clipboard Client", nullptr));
        clientGroup->setTitle(QCoreApplication::translate("MainWindow", "Connection Settings", nullptr));
        serverLabel->setText(QCoreApplication::translate("MainWindow", "Server:", nullptr));
        serverAddressEdit->setText(QCoreApplication::translate("MainWindow", "localhost", nullptr));
        portLabel->setText(QCoreApplication::translate("MainWindow", "Port:", nullptr));
        usernameLabel->setText(QCoreApplication::translate("MainWindow", "Username:", nullptr));
        passwordLabel->setText(QCoreApplication::translate("MainWindow", "Password:", nullptr));
        btnConnect->setText(QCoreApplication::translate("MainWindow", "Connect", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
