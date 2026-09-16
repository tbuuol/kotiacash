/********************************************************************************
** Form generated from reading UI file 'stealthaddressdialog.ui'
**
** Created by: Qt User Interface Compiler version 5.15.17
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_STEALTHADDRESSDIALOG_H
#define UI_STEALTHADDRESSDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_StealthAddressDialog
{
public:
    QVBoxLayout *verticalLayout;
    QLabel *titleLabel;
    QLabel *descriptionLabel;
    QTableWidget *addressList;
    QLabel *countLabel;
    QHBoxLayout *buttonLayout;
    QPushButton *newAddressButton;
    QPushButton *copyAddressButton;
    QPushButton *deleteAddressButton;
    QSpacerItem *horizontalSpacer;
    QPushButton *sendButton;

    void setupUi(QDialog *StealthAddressDialog)
    {
        if (StealthAddressDialog->objectName().isEmpty())
            StealthAddressDialog->setObjectName(QString::fromUtf8("StealthAddressDialog"));
        StealthAddressDialog->resize(700, 500);
        verticalLayout = new QVBoxLayout(StealthAddressDialog);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        titleLabel = new QLabel(StealthAddressDialog);
        titleLabel->setObjectName(QString::fromUtf8("titleLabel"));
        QFont font;
        font.setPointSize(14);
        font.setWeight(700);
        titleLabel->setFont(font);

        verticalLayout->addWidget(titleLabel);

        descriptionLabel = new QLabel(StealthAddressDialog);
        descriptionLabel->setObjectName(QString::fromUtf8("descriptionLabel"));
        descriptionLabel->setWordWrap(true);

        verticalLayout->addWidget(descriptionLabel);

        addressList = new QTableWidget(StealthAddressDialog);
        addressList->setObjectName(QString::fromUtf8("addressList"));
        addressList->setEditTriggers(QAbstractItemView::NoEditTriggers);
        addressList->setAlternatingRowColors(true);

        verticalLayout->addWidget(addressList);

        countLabel = new QLabel(StealthAddressDialog);
        countLabel->setObjectName(QString::fromUtf8("countLabel"));

        verticalLayout->addWidget(countLabel);

        buttonLayout = new QHBoxLayout();
        buttonLayout->setObjectName(QString::fromUtf8("buttonLayout"));
        newAddressButton = new QPushButton(StealthAddressDialog);
        newAddressButton->setObjectName(QString::fromUtf8("newAddressButton"));

        buttonLayout->addWidget(newAddressButton);

        copyAddressButton = new QPushButton(StealthAddressDialog);
        copyAddressButton->setObjectName(QString::fromUtf8("copyAddressButton"));

        buttonLayout->addWidget(copyAddressButton);

        deleteAddressButton = new QPushButton(StealthAddressDialog);
        deleteAddressButton->setObjectName(QString::fromUtf8("deleteAddressButton"));

        buttonLayout->addWidget(deleteAddressButton);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        buttonLayout->addItem(horizontalSpacer);

        sendButton = new QPushButton(StealthAddressDialog);
        sendButton->setObjectName(QString::fromUtf8("sendButton"));

        buttonLayout->addWidget(sendButton);


        verticalLayout->addLayout(buttonLayout);


        retranslateUi(StealthAddressDialog);

        QMetaObject::connectSlotsByName(StealthAddressDialog);
    } // setupUi

    void retranslateUi(QDialog *StealthAddressDialog)
    {
        StealthAddressDialog->setWindowTitle(QCoreApplication::translate("StealthAddressDialog", "Stealth Addresses", nullptr));
        titleLabel->setText(QCoreApplication::translate("StealthAddressDialog", "Stealth Addresses - Anonymous Receiving", nullptr));
        descriptionLabel->setText(QCoreApplication::translate("StealthAddressDialog", "Stealth addresses allow you to receive payments anonymously.\n"
"Each payment uses a unique one-time address that only you can detect.", nullptr));
        countLabel->setText(QCoreApplication::translate("StealthAddressDialog", "0 addresses", nullptr));
        newAddressButton->setText(QCoreApplication::translate("StealthAddressDialog", "New Address", nullptr));
        copyAddressButton->setText(QCoreApplication::translate("StealthAddressDialog", "Copy Address", nullptr));
        deleteAddressButton->setText(QCoreApplication::translate("StealthAddressDialog", "Delete", nullptr));
        sendButton->setText(QCoreApplication::translate("StealthAddressDialog", "Send to Stealth", nullptr));
    } // retranslateUi

};

namespace Ui {
    class StealthAddressDialog: public Ui_StealthAddressDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_STEALTHADDRESSDIALOG_H
