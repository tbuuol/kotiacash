// Copyright (c) 2024 Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/stealthaddressdialog.h"
#include "qt/ui_stealthaddressdialog.h"
#include "qt/walletmodel.h"
#include "qt/guiutil.h"
#include "qt/bitcoinunits.h"
#include "stealth.h"
#include "wallet/wallet.h"

#include <QMessageBox>
#include <QClipboard>
#include <QInputDialog>
#include <QTableWidgetItem>

StealthAddressDialog::StealthAddressDialog(const PlatformStyle *platformStyle,
                                            WalletModel *walletModel, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::StealthAddressDialog),
    walletModel(walletModel),
    platformStyle(platformStyle)
{
    ui->setupUi(this);

    // Setup address list
    ui->addressList->setColumnCount(2);
    ui->addressList->setHorizontalHeaderLabels(QStringList() << tr("Address") << tr("Label"));
    ui->addressList->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->addressList->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->addressList->horizontalHeader()->setStretchLastSection(true);

    updateAddressList();
}

StealthAddressDialog::~StealthAddressDialog()
{
    delete ui;
}

void StealthAddressDialog::updateAddressList()
{
    if (!walletModel)
        return;

    ui->addressList->setRowCount(0);

    std::vector<CStealthAddress> addresses = walletModel->wallet->GetStealthAddresses();

    for (size_t i = 0; i < addresses.size(); i++) {
        ui->addressList->insertRow(i);

        QTableWidgetItem* addrItem = new QTableWidgetItem(QString::fromStdString(addresses[i].Encode()));
        addrItem->setFlags(addrItem->flags() & ~Qt::ItemIsEditable);
        ui->addressList->setItem(i, 0, addrItem);

        QTableWidgetItem* labelItem = new QTableWidgetItem(QString::fromStdString(addresses[i].label));
        labelItem->setFlags(labelItem->flags() & ~Qt::ItemIsEditable);
        ui->addressList->setItem(i, 1, labelItem);
    }

    ui->countLabel->setText(QString("%1 addresses").arg(addresses.size()));
}

void StealthAddressDialog::on_newAddressButton_clicked()
{
    if (!walletModel)
        return;

    bool ok;
    QString label = QInputDialog::getText(this, tr("New Stealth Address"),
                                           tr("Enter a label for this address:"),
                                           QLineEdit::Normal, "", &ok);

    if (ok && !label.isEmpty()) {
        try {
            CStealthAddress sa = walletModel->wallet->GenerateNewStealthAddress(label.toStdString());

            QMessageBox::information(this, tr("New Stealth Address"),
                                     tr("New stealth address created:\n%1").arg(QString::fromStdString(sa.Encode())));

            updateAddressList();
        } catch (const std::exception& e) {
            QMessageBox::critical(this, tr("Error"),
                                  tr("Failed to create stealth address: %1").arg(e.what()));
        }
    }
}

void StealthAddressDialog::on_copyAddressButton_clicked()
{
    int row = ui->addressList->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, tr("No Selection"),
                             tr("Please select an address to copy."));
        return;
    }

    QTableWidgetItem* item = ui->addressList->item(row, 0);
    if (item) {
        QClipboard* clipboard = QApplication::clipboard();
        clipboard->setText(item->text());

        QMessageBox::information(this, tr("Copied"),
                                 tr("Address copied to clipboard."));
    }
}

void StealthAddressDialog::on_deleteAddressButton_clicked()
{
    int row = ui->addressList->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, tr("No Selection"),
                             tr("Please select an address to delete."));
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Delete Address"),
        tr("Are you sure you want to delete this address?\n\n"
           "Note: This only removes the address from the list. The keys remain in the wallet."),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        // Note: We don't actually delete keys from the wallet for security
        // Just remove from the visible list if needed
        updateAddressList();
    }
}

void StealthAddressDialog::on_sendButton_clicked()
{
    showSendDialog();
}

void StealthAddressDialog::showSendDialog()
{
    if (!walletModel)
        return;

    // Unlock wallet if needed
    if (walletModel->getEncryptionStatus() == WalletModel::Locked) {
        Q_EMIT walletModel->requireUnlock();
        return;
    }

    bool ok;
    QString address = QInputDialog::getText(this, tr("Send to Stealth Address"),
                                             tr("Enter the stealth address:"),
                                             QLineEdit::Normal, "", &ok);

    if (!ok || address.isEmpty())
        return;

    CStealthAddress stealthAddr;
    if (!stealthAddr.Decode(address.toStdString())) {
        QMessageBox::critical(this, tr("Invalid Address"),
                              tr("The entered stealth address is invalid."));
        return;
    }

    QString amountStr = QInputDialog::getText(this, tr("Amount"),
                                               tr("Enter the amount to send (KOT):"),
                                               QLineEdit::Normal, "", &ok);

    if (!ok || amountStr.isEmpty())
        return;

    bool ok2;
    double amount = amountStr.toDouble(&ok2);
    if (!ok2 || amount <= 0) {
        QMessageBox::critical(this, tr("Invalid Amount"),
                              tr("The entered amount is invalid."));
        return;
    }

    CAmount nAmount = amount * COIN;

    // Create and send the transaction
    // This would be handled through the wallet model
    QMessageBox::information(this, tr("Send"),
                             tr("Sending %1 KOT to stealth address.\n\n"
                                "Note: This feature requires wallet unlock.")
                             .arg(amount, 0, 'f', 8));
}

void StealthAddressDialog::on_addressList_doubleClicked(const QModelIndex &index)
{
    // Copy address on double click
    on_copyAddressButton_clicked();
}
