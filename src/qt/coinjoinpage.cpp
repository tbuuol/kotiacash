// Copyright (c) 2024 Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/coinjoinpage.h"
#include "qt/coinjoinworker.h"
#include "qt/ui_coinjoinpage.h"
#include "qt/walletmodel.h"
#include "qt/clientmodel.h"
#include "qt/guiutil.h"
#include "qt/bitcoinunits.h"
#include "qt/optionsmodel.h"
#include "coinjoin.h"
#include "wallet/wallet.h"
#include "chainparams.h"
#include "util.h"

#include <QMessageBox>
#include <QSettings>
#include <QThread>

CoinJoinPage::CoinJoinPage(const PlatformStyle *platformStyle, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CoinJoinPage),
    clientModel(0),
    walletModel(0),
    platformStyle(platformStyle),
    timer(new QTimer(this)),
    m_workerThread(nullptr),
    m_worker(nullptr)
{
    ui->setupUi(this);

    // Setup denomination combo box - editable so users can type custom max tx amount
    ui->denominationComboBox->setEditable(true);
    ui->denominationComboBox->addItem("0.001 KOT", (qlonglong)COINJOIN_DENOM_1);
    ui->denominationComboBox->addItem("0.01 KOT", (qlonglong)COINJOIN_DENOM_2);
    ui->denominationComboBox->addItem("0.1 KOT", (qlonglong)COINJOIN_DENOM_3);
    ui->denominationComboBox->addItem("1 KOT", (qlonglong)COINJOIN_DENOM_4);
    ui->denominationComboBox->addItem("10 KOT", (qlonglong)COINJOIN_DENOM_5);
    ui->denominationComboBox->setInsertPolicy(QComboBox::NoInsert);

    // Setup rounds spin box
    ui->roundsSpinBox->setMinimum(1);
    ui->roundsSpinBox->setMaximum(16);
    ui->roundsSpinBox->setValue(COINJOIN_DEFAULT_ROUNDS);

    // Connect mix max button
    connect(ui->mixMaxButton, SIGNAL(clicked()), this, SLOT(on_mixMaxButton_clicked()));

    // Connect timer for status updates
    connect(timer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    timer->start(5000); // Update every 5 seconds

    // Load settings
    QSettings settings;
    ui->enableCoinJoinCheckbox->setChecked(settings.value("fCoinJoinEnabled", false).toBool());
    ui->roundsSpinBox->setValue(settings.value("nCoinJoinRounds", COINJOIN_DEFAULT_ROUNDS).toInt());
}

void CoinJoinPage::setWalletModel(WalletModel *walletModel)
{
    this->walletModel = walletModel;

    if (walletModel) {
        connect(walletModel, SIGNAL(balanceChanged(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)),
                this, SLOT(setBalance(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)));

        // Initialize staking checkbox
        ui->enableStakingCheckbox->setChecked(walletModel->isStakingEnabled());

        // Auto-select best denomination based on available coins
        CAmount suggested = walletModel->wallet->SuggestCoinJoinDenomination(false);
        if (suggested == 0) {
            suggested = walletModel->wallet->SuggestCoinJoinDenomination(true);
        }
        if (suggested > 0) {
            int idx = ui->denominationComboBox->findData((qlonglong)suggested);
            if (idx >= 0) {
                ui->denominationComboBox->setCurrentIndex(idx);
            }
        }

        // Create background worker thread for CoinJoin mixing
        m_workerThread = new QThread(this);
        m_worker = new CoinJoinWorker(walletModel->wallet);
        m_worker->moveToThread(m_workerThread);

        // Connect worker signals to UI slots
        connect(m_worker, &CoinJoinWorker::statusChanged, this, &CoinJoinPage::onMixingStatusChanged);
        connect(m_worker, &CoinJoinWorker::mixingFinished, this, &CoinJoinPage::onMixingFinished);
        connect(m_worker, &CoinJoinWorker::mixingPartial, this, &CoinJoinPage::onMixingPartial);
        connect(m_worker, &CoinJoinWorker::mixingFailed, this, &CoinJoinPage::onMixingFailed);

        // Start the worker thread
        m_workerThread->start();

        // Initial staking info update
        updateStakingInfo();
    }
}

CoinJoinPage::~CoinJoinPage()
{
    // Stop and clean up worker thread
    if (m_worker) {
        m_worker->requestStop();
    }
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait(5000);
    }
    delete ui;
}

void CoinJoinPage::setClientModel(ClientModel *clientModel)
{
    this->clientModel = clientModel;
}

void CoinJoinPage::setBalance(const CAmount& balance, const CAmount& unconfirmedBalance,
                               const CAmount& immatureBalance, const CAmount& stake,
                               const CAmount&, const CAmount&, const CAmount&, const CAmount&)
{
    if (!walletModel)
        return;

    int unit = walletModel->getOptionsModel()->getDisplayUnit();

    // Available = confirmed balance (excluding stake)
    ui->availableBalanceLabel->setText(
        QString("Available: %1").arg(BitcoinUnits::formatWithUnit(unit, balance)));

    // Unconfirmed
    ui->unconfirmedBalanceLabel->setText(
        QString("Unconfirmed: %1").arg(BitcoinUnits::formatWithUnit(unit, unconfirmedBalance)));

    // Stake balance
    ui->stakeBalanceLabel->setText(
        QString("Stake: %1").arg(BitcoinUnits::formatWithUnit(unit, stake)));

    // Total = all balances combined
    CAmount total = balance + unconfirmedBalance + immatureBalance + stake;
    ui->totalBalanceLabel->setText(
        QString("Total: %1").arg(BitcoinUnits::formatWithUnit(unit, total)));

    // Mixable balance = available balance (reserve 0.001 KOT for fees)
    CAmount feeReserve = 100000; // 0.001 KOT
    CAmount mixable = balance > feeReserve ? balance - feeReserve : 0;
    ui->mixableBalanceLabel->setText(
        QString("Mixable: %1").arg(BitcoinUnits::formatWithUnit(unit, mixable)));

    // Update mix amount spinbox max
    ui->mixAmountSpinBox->setMaximum((double)mixable / COIN);

    updateProgressBars();
    updateStakingInfo();
}

void CoinJoinPage::on_startMixButton_clicked()
{
    if (!walletModel)
        return;

    // Check if already mixing
    if (m_worker && m_worker->isMixing()) {
        QMessageBox::information(this, tr("CoinJoin"), tr("Mixing is already in progress."));
        return;
    }

    // Get denomination: from combo data if preset, or parse custom text input
    CAmount denomination = (CAmount)ui->denominationComboBox->currentData().toLongLong();
    if (denomination == 0) {
        // Parse custom value from editable combo box text
        QString text = ui->denominationComboBox->currentText().trimmed();
        bool ok = false;
        double val = text.toDouble(&ok);
        if (!ok || val <= 0) {
            QMessageBox::warning(this, tr("CoinJoin Error"),
                tr("Please select or enter a valid Max Transaction Amount."));
            return;
        }
        denomination = (CAmount)(val * COIN);
    }

    int rounds = ui->roundsSpinBox->value();
    CAmount mixAmount = (CAmount)(ui->mixAmountSpinBox->value() * COIN);

    // Validate mix amount
    if (mixAmount <= 0) {
        QMessageBox::warning(this, tr("CoinJoin Error"),
            tr("Please enter a mix amount greater than 0."));
        return;
    }

    // Check if max transaction amount is enough to create at least two varied outputs plus fee.
    CAmount fee = COINJOIN_DEFAULT_FEE;
    CAmount minDenom = 2 * COINJOIN_MIN_MIX_OUTPUT + fee; // two outputs above dust + fee
    if (denomination < minDenom) {
        int unit = walletModel->getOptionsModel()->getDisplayUnit();
        QMessageBox::warning(this, tr("CoinJoin Error"),
            tr("Max transaction amount is too small.\n"
               "Minimum: %1 including fee %2")
                .arg(BitcoinUnits::formatWithUnit(unit, minDenom))
                .arg(BitcoinUnits::formatWithUnit(unit, fee)));
        return;
    }

    // Confirm with user
    int unit = walletModel->getOptionsModel()->getDisplayUnit();
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Start CoinJoin Mixing"),
        tr("Create random CoinJoin transactions up to %1 each, split into varied outputs, for %2 rounds?\n\n"
           "Mixable amount: %3\n"
           "Fee per round: %4\n"
           "Rounds requested: %5 transactions")
            .arg(BitcoinUnits::formatWithUnit(unit, denomination))
            .arg(rounds)
            .arg(BitcoinUnits::formatWithUnit(unit, mixAmount))
            .arg(BitcoinUnits::formatWithUnit(unit, fee))
            .arg(rounds),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        // Unlock wallet if needed
        if (walletModel->getEncryptionStatus() == WalletModel::Locked) {
            Q_EMIT walletModel->requireUnlock();
            return;
        }

        ui->coinjoinStatusLabel->setText(tr("Starting mixing in background..."));
        ui->startMixButton->setEnabled(false);

        // Start mixing in background thread - UI stays responsive
        if (m_worker) {
            m_worker->startMixing(denomination, rounds, mixAmount);
        }
    }
}

void CoinJoinPage::on_stopMixButton_clicked()
{
    if (!walletModel)
        return;

    if (m_worker && m_worker->isMixing()) {
        m_worker->requestStop();
        ui->coinjoinStatusLabel->setText(tr("Mixing stopping..."));
        LogPrintf("CoinJoin UI: Stop mixing requested\n");
    }
}

void CoinJoinPage::on_mixMaxButton_clicked()
{
    if (!walletModel)
        return;

    // Get mixable balance (available - fee reserve)
    CAmount balance = walletModel->getBalance();
    CAmount feeReserve = 100000; // 0.001 KOT
    CAmount mixable = balance > feeReserve ? balance - feeReserve : 0;

    ui->mixAmountSpinBox->setValue((double)mixable / COIN);
}

void CoinJoinPage::on_enableCoinJoinCheckbox_toggled(bool checked)
{
    if (!walletModel)
        return;

    walletModel->wallet->SetCoinJoinEnabled(checked);

    QSettings settings;
    settings.setValue("fCoinJoinEnabled", checked);

    ui->coinjoinStatusLabel->setText(checked ? tr("CoinJoin enabled") : tr("CoinJoin disabled"));
}

void CoinJoinPage::on_enableStakingCheckbox_toggled(bool checked)
{
    if (!walletModel)
        return;

    walletModel->setStakingEnabled(checked);
    updateStakingInfo();

    QSettings settings;
    settings.setValue("fStakingEnabled", checked);
}

void CoinJoinPage::on_roundsSpinBox_valueChanged(int value)
{
    if (!walletModel)
        return;

    walletModel->wallet->SetCoinJoinRounds(value);

    QSettings settings;
    settings.setValue("nCoinJoinRounds", value);
}

void CoinJoinPage::on_denominationComboBox_currentIndexChanged(int index)
{
    updateProgressBars();
}

void CoinJoinPage::updateStatus()
{
    if (!walletModel)
        return;

    CAmount denomination = (CAmount)ui->denominationComboBox->currentData().toLongLong();
    double progress = walletModel->wallet->GetCoinJoinProgress(denomination);

    ui->progressBar->setValue((int)progress);
    ui->progressLabel->setText(QString("%1%").arg(progress, 0, 'f', 1));

    QString lastStatus = QString::fromStdString(walletModel->wallet->GetCoinJoinLastStatus());
    int requested = walletModel->wallet->GetCoinJoinLastRoundsRequested();
    int completed = walletModel->wallet->GetCoinJoinLastRoundsCompleted();
    int txRequested = walletModel->wallet->GetCoinJoinLastTransactionsRequested();
    int txCompleted = walletModel->wallet->GetCoinJoinLastTransactionsCompleted();

    if (walletModel->wallet->IsCoinJoinMixing()) {
        if (!lastStatus.isEmpty()) {
            ui->coinjoinStatusLabel->setText(lastStatus);
        } else {
            ui->coinjoinStatusLabel->setText(tr("Mixing in progress..."));
        }
    } else if (txRequested > 0 && !lastStatus.isEmpty()) {
        ui->coinjoinStatusLabel->setText(QString("%1 (%2/%3 rounds, %4/%5 tx)")
            .arg(lastStatus)
            .arg(completed)
            .arg(requested)
            .arg(txCompleted)
            .arg(txRequested));
    } else if (walletModel->wallet->IsCoinJoinEnabled()) {
        ui->coinjoinStatusLabel->setText(tr("CoinJoin enabled - ready"));
    } else {
        ui->coinjoinStatusLabel->setText(tr("CoinJoin disabled"));
    }

    updateProgressBars();
    updateStakingInfo();
}

void CoinJoinPage::updateStakingInfo()
{
    if (!walletModel)
        return;

    bool enabled = walletModel->isStakingEnabled();
    bool active = walletModel->isStakingActive();
    uint64_t weight = walletModel->getStakeWeight();
    uint64_t netWeight = walletModel->getNetworkStakeWeight();
    int64_t interval = walletModel->getStakingSearchInterval();

    // Staking status
    if (!enabled) {
        ui->stakingStatusLabel->setText(tr("Staking: Disabled"));
    } else if (!active) {
        if (walletModel->getEncryptionStatus() == WalletModel::Locked) {
            ui->stakingStatusLabel->setText(tr("Staking: Wallet locked"));
        } else if (interval == 0) {
            ui->stakingStatusLabel->setText(tr("Staking: Waiting for mature coins..."));
        } else {
            ui->stakingStatusLabel->setText(tr("Staking: Searching..."));
        }
    } else {
        ui->stakingStatusLabel->setText(tr("Staking: Active"));
    }

    // Weight info
    QString weightText;
    if (weight > 0) {
        double weightCoin = (double)weight / COIN;
        double netWeightCoin = (double)netWeight / COIN;
        weightText = QString("Your Weight: %1 | Network Weight: %2")
            .arg(weightCoin, 0, 'f', 2)
            .arg(netWeightCoin, 0, 'f', 2);
    } else {
        weightText = tr("Your Weight: 0 | Network Weight: %1")
            .arg((double)netWeight / COIN, 0, 'f', 2);
    }
    ui->stakingWeightLabel->setText(weightText);

    // Expected time
    QString expectedText;
    if (enabled && active && weight > 0 && netWeight > 0) {
        // Expected time = targetSpacing * networkWeight / weight
        int64_t targetSpacing = Params().GetConsensus().nTargetSpacing;
        uint64_t expectedSeconds = (uint64_t)targetSpacing * netWeight / weight;

        if (expectedSeconds < 60) {
            expectedText = tr("Expected Time: ~%1 sec").arg(expectedSeconds);
        } else if (expectedSeconds < 3600) {
            expectedText = tr("Expected Time: ~%1 min").arg(expectedSeconds / 60);
        } else if (expectedSeconds < 86400) {
            expectedText = tr("Expected Time: ~%1 hours").arg(expectedSeconds / 3600);
        } else {
            expectedText = tr("Expected Time: ~%1 days").arg(expectedSeconds / 86400);
        }
    } else {
        expectedText = tr("Expected Time: --");
    }
    ui->stakingExpectedLabel->setText(expectedText);
}

void CoinJoinPage::updateProgressBars()
{
    if (!walletModel)
        return;

    std::vector<CAmount> denoms = GetStandardDenominations();
    QStringList labels;
    QStringList values;

    for (size_t i = 0; i < denoms.size() && i < 5; i++) {
        double progress = walletModel->wallet->GetCoinJoinProgress(denoms[i]);
        labels.append(QString("%1 KOT").arg((double)denoms[i] / COIN, 0, 'f', 3));
        values.append(QString("%1%").arg(progress, 0, 'f', 1));
    }

    ui->denom1Label->setText(labels.value(0));
    ui->denom1Progress->setText(values.value(0));
    ui->denom2Label->setText(labels.value(1));
    ui->denom2Progress->setText(values.value(1));
    ui->denom3Label->setText(labels.value(2));
    ui->denom3Progress->setText(values.value(2));
    ui->denom4Label->setText(labels.value(3));
    ui->denom4Progress->setText(values.value(3));
    ui->denom5Label->setText(labels.value(4));
    ui->denom5Progress->setText(values.value(4));
}

void CoinJoinPage::updateDisplayUnit()
{
    if (walletModel && walletModel->getOptionsModel()) {
        setBalance(walletModel->getBalance(),
                   walletModel->getUnconfirmedBalance(),
                   walletModel->getImmatureBalance(),
                   walletModel->getStake(),
                   0, 0, 0, 0);
    }
}

/** Worker slot: called from background thread when status changes */
void CoinJoinPage::onMixingStatusChanged(const QString& status)
{
    ui->coinjoinStatusLabel->setText(status);
    updateProgressBars();
}

/** Worker slot: called when mixing completes successfully */
void CoinJoinPage::onMixingFinished(int created, int requested)
{
    ui->startMixButton->setEnabled(true);

    QString lastStatus = QString::fromStdString(walletModel->wallet->GetCoinJoinLastStatus());
    ui->coinjoinStatusLabel->setText(lastStatus);
    LogPrintf("CoinJoin UI: Mixing completed with %d/%d transactions\n", created, requested);

    updateProgressBars();
}

/** Worker slot: called when mixing partially completes */
void CoinJoinPage::onMixingPartial(int created, int requested, const QString& status)
{
    ui->startMixButton->setEnabled(true);

    ui->coinjoinStatusLabel->setText(status);
    QMessageBox::warning(this, tr("CoinJoin Partial"),
        tr("Only %1 of %2 requested CoinJoin transactions were created.\n%3")
            .arg(created)
            .arg(requested)
            .arg(status));
    LogPrintf("CoinJoin UI: Mixing partial with %d/%d transactions\n", created, requested);

    updateProgressBars();
}

/** Worker slot: called when mixing fails */
void CoinJoinPage::onMixingFailed(const QString& error)
{
    ui->startMixButton->setEnabled(true);

    ui->coinjoinStatusLabel->setText(error.isEmpty() ? tr("Mixing failed") : error);

    // Show detailed error with available balance and suggestion
    CAmount denomination = (CAmount)ui->denominationComboBox->currentData().toLongLong();
    std::pair<CAmount, int> bal = walletModel->wallet->GetCoinJoinAvailableBalance(false);
    CAmount suggested = walletModel->wallet->SuggestCoinJoinDenomination(false);
    if (bal.first == 0) {
        bal = walletModel->wallet->GetCoinJoinAvailableBalance(true);
        suggested = walletModel->wallet->SuggestCoinJoinDenomination(true);
    }

    int unit = walletModel->getOptionsModel()->getDisplayUnit();
    QString errorMsg;
    if (bal.first == 0) {
        errorMsg = tr("No coins available in wallet. Please receive some KOT first.");
    } else if (suggested == 0) {
        errorMsg = tr("Balance too low for mixing. Need at least 2x the denomination.");
    } else if (suggested < denomination) {
        errorMsg = tr("No single coin >= %1 KOT.\n"
                      "Available: %2 (%3 coins)\n"
                      "Try denomination: %4 KOT")
            .arg((double)denomination / COIN, 0, 'f', 3)
            .arg(BitcoinUnits::formatWithUnit(unit, bal.first))
            .arg(bal.second)
            .arg((double)suggested / COIN, 0, 'f', 3);
    } else {
        errorMsg = tr("No suitable coins found. Check debug log for details.");
    }

    QMessageBox::warning(this, tr("CoinJoin Error"), errorMsg);
    LogPrintf("CoinJoin UI: Mixing failed - %s\n", error.toStdString().c_str());

    updateProgressBars();
}
