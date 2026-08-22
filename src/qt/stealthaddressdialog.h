// Copyright (c) 2024 Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_STEALTHADDRESSDIALOG_H
#define BITCOIN_QT_STEALTHADDRESSDIALOG_H

#include <QDialog>
#include <QModelIndex>

class WalletModel;
class PlatformStyle;

namespace Ui {
    class StealthAddressDialog;
}

/**
 * Dialog for managing stealth addresses
 */
class StealthAddressDialog : public QDialog
{
    Q_OBJECT

    friend class WalletModel;

public:
    explicit StealthAddressDialog(const PlatformStyle *platformStyle, WalletModel *walletModel, QWidget *parent = 0);
    ~StealthAddressDialog();

private Q_SLOTS:
    void on_newAddressButton_clicked();
    void on_copyAddressButton_clicked();
    void on_deleteAddressButton_clicked();
    void on_sendButton_clicked();
    void on_addressList_doubleClicked(const QModelIndex &index);
    void updateAddressList();

private:
    Ui::StealthAddressDialog *ui;
    WalletModel *walletModel;
    const PlatformStyle *platformStyle;

    void showNewAddressDialog();
    void showSendDialog();
};

#endif // BITCOIN_QT_STEALTHADDRESSDIALOG_H
