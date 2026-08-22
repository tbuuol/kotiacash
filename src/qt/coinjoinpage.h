// Copyright (c) 2024 Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_COINJOINPAGE_H
#define BITCOIN_QT_COINJOINPAGE_H

#include "amount.h"
#include <QWidget>
#include <QTimer>

class ClientModel;
class WalletModel;
class PlatformStyle;
class CoinJoinWorker;
class QThread;

namespace Ui {
    class CoinJoinPage;
}

/**
 * CoinJoin page widget
 * Provides UI for managing CoinJoin mixing operations
 */
class CoinJoinPage : public QWidget
{
    Q_OBJECT

public:
    explicit CoinJoinPage(const PlatformStyle *platformStyle, QWidget *parent = 0);
    ~CoinJoinPage();

    void setClientModel(ClientModel *clientModel);
    void setWalletModel(WalletModel *walletModel);

public Q_SLOTS:
	void setBalance(const CAmount& balance, const CAmount& unconfirmedBalance,
                     const CAmount& immatureBalance, const CAmount& stake,
                     const CAmount& watchOnlyBalance, const CAmount& watchUnconfBalance,
                     const CAmount& watchImmatureBalance, const CAmount& watchOnlyStake);

private Q_SLOTS:
    void on_startMixButton_clicked();
    void on_stopMixButton_clicked();
    void on_mixMaxButton_clicked();
    void on_enableCoinJoinCheckbox_toggled(bool checked);
    void on_enableStakingCheckbox_toggled(bool checked);
    void on_roundsSpinBox_valueChanged(int value);
    void on_denominationComboBox_currentIndexChanged(int index);
    void updateStatus();
    void updateStakingInfo();
    void updateDisplayUnit();

    /** Slot for worker status updates */
    void onMixingStatusChanged(const QString& status);
    void onMixingFinished(int created, int requested);
    void onMixingPartial(int created, int requested, const QString& status);
    void onMixingFailed(const QString& error);

private:
    Ui::CoinJoinPage *ui;
    ClientModel *clientModel;
    WalletModel *walletModel;
    const PlatformStyle *platformStyle;
    QTimer *timer;

    /** Background worker thread for CoinJoin mixing */
    QThread* m_workerThread;
    CoinJoinWorker* m_worker;

    void updateProgressBars();
    void updateDenominations();
};

#endif // BITCOIN_QT_COINJOINPAGE_H
