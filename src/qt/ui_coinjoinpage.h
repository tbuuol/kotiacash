/********************************************************************************
** Form generated from reading UI file 'coinjoinpage.ui'
**
** Created by: Qt User Interface Compiler version 5.15.18
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_COINJOINPAGE_H
#define UI_COINJOINPAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_CoinJoinPage
{
public:
    QVBoxLayout *verticalLayout;
    QLabel *titleLabel;
    QGroupBox *statusGroupBox;
    QFormLayout *formLayout;
    QLabel *coinjoinStatusLabel;
    QLabel *availableBalanceLabel;
    QLabel *unconfirmedBalanceLabel;
    QLabel *stakeBalanceLabel;
    QLabel *totalBalanceLabel;
    QGroupBox *stakingGroupBox;
    QFormLayout *formLayout_3;
    QCheckBox *enableStakingCheckbox;
    QLabel *stakingStatusLabel;
    QLabel *stakingWeightLabel;
    QLabel *stakingExpectedLabel;
    QGroupBox *settingsGroupBox;
    QFormLayout *formLayout_2;
    QCheckBox *enableCoinJoinCheckbox;
    QLabel *denominationLabel;
    QComboBox *denominationComboBox;
    QLabel *roundsLabel;
    QSpinBox *roundsSpinBox;
    QLabel *mixAmountLabel;
    QHBoxLayout *mixAmountLayout;
    QDoubleSpinBox *mixAmountSpinBox;
    QPushButton *mixMaxButton;
    QLabel *mixableBalanceLabel;
    QGroupBox *progressGroupBox;
    QVBoxLayout *verticalLayout_2;
    QProgressBar *progressBar;
    QLabel *progressLabel;
    QGroupBox *denominationsGroupBox;
    QGridLayout *gridLayout;
    QLabel *denom1Label;
    QLabel *denom1Progress;
    QLabel *denom2Label;
    QLabel *denom2Progress;
    QLabel *denom3Label;
    QLabel *denom3Progress;
    QLabel *denom4Label;
    QLabel *denom4Progress;
    QLabel *denom5Label;
    QLabel *denom5Progress;
    QHBoxLayout *buttonLayout;
    QPushButton *startMixButton;
    QPushButton *stopMixButton;
    QSpacerItem *verticalSpacer;

    void setupUi(QWidget *CoinJoinPage)
    {
        if (CoinJoinPage->objectName().isEmpty())
            CoinJoinPage->setObjectName(QString::fromUtf8("CoinJoinPage"));
        CoinJoinPage->resize(800, 700);
        verticalLayout = new QVBoxLayout(CoinJoinPage);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        titleLabel = new QLabel(CoinJoinPage);
        titleLabel->setObjectName(QString::fromUtf8("titleLabel"));
        QFont font;
        font.setPointSize(16);
        font.setWeight(700);
        titleLabel->setFont(font);

        verticalLayout->addWidget(titleLabel);

        statusGroupBox = new QGroupBox(CoinJoinPage);
        statusGroupBox->setObjectName(QString::fromUtf8("statusGroupBox"));
        formLayout = new QFormLayout(statusGroupBox);
        formLayout->setObjectName(QString::fromUtf8("formLayout"));
        coinjoinStatusLabel = new QLabel(statusGroupBox);
        coinjoinStatusLabel->setObjectName(QString::fromUtf8("coinjoinStatusLabel"));
        QFont font1;
        font1.setWeight(700);
        coinjoinStatusLabel->setFont(font1);

        formLayout->setWidget(0, QFormLayout::LabelRole, coinjoinStatusLabel);

        availableBalanceLabel = new QLabel(statusGroupBox);
        availableBalanceLabel->setObjectName(QString::fromUtf8("availableBalanceLabel"));
        availableBalanceLabel->setFont(font1);

        formLayout->setWidget(1, QFormLayout::LabelRole, availableBalanceLabel);

        unconfirmedBalanceLabel = new QLabel(statusGroupBox);
        unconfirmedBalanceLabel->setObjectName(QString::fromUtf8("unconfirmedBalanceLabel"));

        formLayout->setWidget(1, QFormLayout::LabelRole, unconfirmedBalanceLabel);

        stakeBalanceLabel = new QLabel(statusGroupBox);
        stakeBalanceLabel->setObjectName(QString::fromUtf8("stakeBalanceLabel"));

        formLayout->setWidget(2, QFormLayout::LabelRole, stakeBalanceLabel);

        totalBalanceLabel = new QLabel(statusGroupBox);
        totalBalanceLabel->setObjectName(QString::fromUtf8("totalBalanceLabel"));
        totalBalanceLabel->setFont(font1);

        formLayout->setWidget(3, QFormLayout::LabelRole, totalBalanceLabel);


        verticalLayout->addWidget(statusGroupBox);

        stakingGroupBox = new QGroupBox(CoinJoinPage);
        stakingGroupBox->setObjectName(QString::fromUtf8("stakingGroupBox"));
        formLayout_3 = new QFormLayout(stakingGroupBox);
        formLayout_3->setObjectName(QString::fromUtf8("formLayout_3"));
        enableStakingCheckbox = new QCheckBox(stakingGroupBox);
        enableStakingCheckbox->setObjectName(QString::fromUtf8("enableStakingCheckbox"));
        enableStakingCheckbox->setChecked(true);

        formLayout_3->setWidget(0, QFormLayout::LabelRole, enableStakingCheckbox);

        stakingStatusLabel = new QLabel(stakingGroupBox);
        stakingStatusLabel->setObjectName(QString::fromUtf8("stakingStatusLabel"));

        formLayout_3->setWidget(1, QFormLayout::LabelRole, stakingStatusLabel);

        stakingWeightLabel = new QLabel(stakingGroupBox);
        stakingWeightLabel->setObjectName(QString::fromUtf8("stakingWeightLabel"));

        formLayout_3->setWidget(2, QFormLayout::LabelRole, stakingWeightLabel);

        stakingExpectedLabel = new QLabel(stakingGroupBox);
        stakingExpectedLabel->setObjectName(QString::fromUtf8("stakingExpectedLabel"));

        formLayout_3->setWidget(3, QFormLayout::LabelRole, stakingExpectedLabel);


        verticalLayout->addWidget(stakingGroupBox);

        settingsGroupBox = new QGroupBox(CoinJoinPage);
        settingsGroupBox->setObjectName(QString::fromUtf8("settingsGroupBox"));
        formLayout_2 = new QFormLayout(settingsGroupBox);
        formLayout_2->setObjectName(QString::fromUtf8("formLayout_2"));
        enableCoinJoinCheckbox = new QCheckBox(settingsGroupBox);
        enableCoinJoinCheckbox->setObjectName(QString::fromUtf8("enableCoinJoinCheckbox"));

        formLayout_2->setWidget(0, QFormLayout::LabelRole, enableCoinJoinCheckbox);

        denominationLabel = new QLabel(settingsGroupBox);
        denominationLabel->setObjectName(QString::fromUtf8("denominationLabel"));

        formLayout_2->setWidget(1, QFormLayout::LabelRole, denominationLabel);

        denominationComboBox = new QComboBox(settingsGroupBox);
        denominationComboBox->setObjectName(QString::fromUtf8("denominationComboBox"));

        formLayout_2->setWidget(1, QFormLayout::FieldRole, denominationComboBox);

        roundsLabel = new QLabel(settingsGroupBox);
        roundsLabel->setObjectName(QString::fromUtf8("roundsLabel"));

        formLayout_2->setWidget(2, QFormLayout::LabelRole, roundsLabel);

        roundsSpinBox = new QSpinBox(settingsGroupBox);
        roundsSpinBox->setObjectName(QString::fromUtf8("roundsSpinBox"));
        roundsSpinBox->setMinimum(1);
        roundsSpinBox->setMaximum(16);
        roundsSpinBox->setValue(4);

        formLayout_2->setWidget(2, QFormLayout::FieldRole, roundsSpinBox);

        mixAmountLabel = new QLabel(settingsGroupBox);
        mixAmountLabel->setObjectName(QString::fromUtf8("mixAmountLabel"));

        formLayout_2->setWidget(3, QFormLayout::LabelRole, mixAmountLabel);

        mixAmountLayout = new QHBoxLayout();
        mixAmountLayout->setObjectName(QString::fromUtf8("mixAmountLayout"));
        mixAmountSpinBox = new QDoubleSpinBox(settingsGroupBox);
        mixAmountSpinBox->setObjectName(QString::fromUtf8("mixAmountSpinBox"));
        mixAmountSpinBox->setDecimals(8);
        mixAmountSpinBox->setMaximum(999999999.000000000000000);

        mixAmountLayout->addWidget(mixAmountSpinBox);

        mixMaxButton = new QPushButton(settingsGroupBox);
        mixMaxButton->setObjectName(QString::fromUtf8("mixMaxButton"));

        mixAmountLayout->addWidget(mixMaxButton);


        formLayout_2->setLayout(3, QFormLayout::FieldRole, mixAmountLayout);

        mixableBalanceLabel = new QLabel(settingsGroupBox);
        mixableBalanceLabel->setObjectName(QString::fromUtf8("mixableBalanceLabel"));

        formLayout_2->setWidget(4, QFormLayout::LabelRole, mixableBalanceLabel);


        verticalLayout->addWidget(settingsGroupBox);

        progressGroupBox = new QGroupBox(CoinJoinPage);
        progressGroupBox->setObjectName(QString::fromUtf8("progressGroupBox"));
        verticalLayout_2 = new QVBoxLayout(progressGroupBox);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        progressBar = new QProgressBar(progressGroupBox);
        progressBar->setObjectName(QString::fromUtf8("progressBar"));
        progressBar->setValue(0);

        verticalLayout_2->addWidget(progressBar);

        progressLabel = new QLabel(progressGroupBox);
        progressLabel->setObjectName(QString::fromUtf8("progressLabel"));
        progressLabel->setAlignment(Qt::AlignCenter);

        verticalLayout_2->addWidget(progressLabel);


        verticalLayout->addWidget(progressGroupBox);

        denominationsGroupBox = new QGroupBox(CoinJoinPage);
        denominationsGroupBox->setObjectName(QString::fromUtf8("denominationsGroupBox"));
        gridLayout = new QGridLayout(denominationsGroupBox);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        denom1Label = new QLabel(denominationsGroupBox);
        denom1Label->setObjectName(QString::fromUtf8("denom1Label"));

        gridLayout->addWidget(denom1Label, 0, 0, 1, 1);

        denom1Progress = new QLabel(denominationsGroupBox);
        denom1Progress->setObjectName(QString::fromUtf8("denom1Progress"));

        gridLayout->addWidget(denom1Progress, 0, 1, 1, 1);

        denom2Label = new QLabel(denominationsGroupBox);
        denom2Label->setObjectName(QString::fromUtf8("denom2Label"));

        gridLayout->addWidget(denom2Label, 1, 0, 1, 1);

        denom2Progress = new QLabel(denominationsGroupBox);
        denom2Progress->setObjectName(QString::fromUtf8("denom2Progress"));

        gridLayout->addWidget(denom2Progress, 1, 1, 1, 1);

        denom3Label = new QLabel(denominationsGroupBox);
        denom3Label->setObjectName(QString::fromUtf8("denom3Label"));

        gridLayout->addWidget(denom3Label, 2, 0, 1, 1);

        denom3Progress = new QLabel(denominationsGroupBox);
        denom3Progress->setObjectName(QString::fromUtf8("denom3Progress"));

        gridLayout->addWidget(denom3Progress, 2, 1, 1, 1);

        denom4Label = new QLabel(denominationsGroupBox);
        denom4Label->setObjectName(QString::fromUtf8("denom4Label"));

        gridLayout->addWidget(denom4Label, 3, 0, 1, 1);

        denom4Progress = new QLabel(denominationsGroupBox);
        denom4Progress->setObjectName(QString::fromUtf8("denom4Progress"));

        gridLayout->addWidget(denom4Progress, 3, 1, 1, 1);

        denom5Label = new QLabel(denominationsGroupBox);
        denom5Label->setObjectName(QString::fromUtf8("denom5Label"));

        gridLayout->addWidget(denom5Label, 4, 0, 1, 1);

        denom5Progress = new QLabel(denominationsGroupBox);
        denom5Progress->setObjectName(QString::fromUtf8("denom5Progress"));

        gridLayout->addWidget(denom5Progress, 4, 1, 1, 1);


        verticalLayout->addWidget(denominationsGroupBox);

        buttonLayout = new QHBoxLayout();
        buttonLayout->setObjectName(QString::fromUtf8("buttonLayout"));
        startMixButton = new QPushButton(CoinJoinPage);
        startMixButton->setObjectName(QString::fromUtf8("startMixButton"));

        buttonLayout->addWidget(startMixButton);

        stopMixButton = new QPushButton(CoinJoinPage);
        stopMixButton->setObjectName(QString::fromUtf8("stopMixButton"));

        buttonLayout->addWidget(stopMixButton);


        verticalLayout->addLayout(buttonLayout);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer);


        retranslateUi(CoinJoinPage);

        QMetaObject::connectSlotsByName(CoinJoinPage);
    } // setupUi

    void retranslateUi(QWidget *CoinJoinPage)
    {
        CoinJoinPage->setWindowTitle(QCoreApplication::translate("CoinJoinPage", "CoinJoin & Staking", nullptr));
        titleLabel->setText(QCoreApplication::translate("CoinJoinPage", "CoinJoin - Anonymous Transactions", nullptr));
        statusGroupBox->setTitle(QCoreApplication::translate("CoinJoinPage", "Wallet Balance", nullptr));
        coinjoinStatusLabel->setText(QCoreApplication::translate("CoinJoinPage", "CoinJoin: Disabled", nullptr));
        availableBalanceLabel->setText(QCoreApplication::translate("CoinJoinPage", "Available: 0 KOT", nullptr));
        unconfirmedBalanceLabel->setText(QCoreApplication::translate("CoinJoinPage", "Unconfirmed: 0 KOT", nullptr));
        stakeBalanceLabel->setText(QCoreApplication::translate("CoinJoinPage", "Stake: 0 KOT", nullptr));
        totalBalanceLabel->setText(QCoreApplication::translate("CoinJoinPage", "Total: 0 KOT", nullptr));
        stakingGroupBox->setTitle(QCoreApplication::translate("CoinJoinPage", "Proof of Stake (PoS)", nullptr));
        enableStakingCheckbox->setText(QCoreApplication::translate("CoinJoinPage", "Enable Staking", nullptr));
        stakingStatusLabel->setText(QCoreApplication::translate("CoinJoinPage", "Staking: Checking...", nullptr));
        stakingWeightLabel->setText(QCoreApplication::translate("CoinJoinPage", "Your Weight: 0 | Network Weight: 0", nullptr));
        stakingExpectedLabel->setText(QCoreApplication::translate("CoinJoinPage", "Expected Time: --", nullptr));
        settingsGroupBox->setTitle(QCoreApplication::translate("CoinJoinPage", "CoinJoin Settings", nullptr));
        enableCoinJoinCheckbox->setText(QCoreApplication::translate("CoinJoinPage", "Enable CoinJoin", nullptr));
        denominationLabel->setText(QCoreApplication::translate("CoinJoinPage", "Max Transaction Amount:", nullptr));
        roundsLabel->setText(QCoreApplication::translate("CoinJoinPage", "Mixing Rounds:", nullptr));
        mixAmountLabel->setText(QCoreApplication::translate("CoinJoinPage", "Mix Amount:", nullptr));
        mixMaxButton->setText(QCoreApplication::translate("CoinJoinPage", "Max", nullptr));
        mixableBalanceLabel->setText(QCoreApplication::translate("CoinJoinPage", "Mixable: 0 KOT", nullptr));
        progressGroupBox->setTitle(QCoreApplication::translate("CoinJoinPage", "Mixing Progress", nullptr));
        progressLabel->setText(QCoreApplication::translate("CoinJoinPage", "0%", nullptr));
        denominationsGroupBox->setTitle(QCoreApplication::translate("CoinJoinPage", "Mixing Progress by Amount", nullptr));
        denom1Label->setText(QCoreApplication::translate("CoinJoinPage", "0.001 KOT", nullptr));
        denom1Progress->setText(QCoreApplication::translate("CoinJoinPage", "0%", nullptr));
        denom2Label->setText(QCoreApplication::translate("CoinJoinPage", "0.01 KOT", nullptr));
        denom2Progress->setText(QCoreApplication::translate("CoinJoinPage", "0%", nullptr));
        denom3Label->setText(QCoreApplication::translate("CoinJoinPage", "0.1 KOT", nullptr));
        denom3Progress->setText(QCoreApplication::translate("CoinJoinPage", "0%", nullptr));
        denom4Label->setText(QCoreApplication::translate("CoinJoinPage", "1 KOT", nullptr));
        denom4Progress->setText(QCoreApplication::translate("CoinJoinPage", "0%", nullptr));
        denom5Label->setText(QCoreApplication::translate("CoinJoinPage", "10 KOT", nullptr));
        denom5Progress->setText(QCoreApplication::translate("CoinJoinPage", "0%", nullptr));
        startMixButton->setText(QCoreApplication::translate("CoinJoinPage", "Start Mixing", nullptr));
        stopMixButton->setText(QCoreApplication::translate("CoinJoinPage", "Stop Mixing", nullptr));
    } // retranslateUi

};

namespace Ui {
    class CoinJoinPage: public Ui_CoinJoinPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_COINJOINPAGE_H
