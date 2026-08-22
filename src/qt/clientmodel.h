// Copyright (c) 2011-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_CLIENTMODEL_H
#define BITCOIN_QT_CLIENTMODEL_H

#include "config.h"

#include <QObject>
#include <QDateTime>
#include <QTimer>

class OptionsModel;
class PeerTableModel;
class BanTableModel;
class Config;

class CBlockIndex;

enum ConnectionDirection {
    CONNECTIONS_NONE = 0,
    CONNECTIONS_IN = (1U << 0),
    CONNECTIONS_OUT = (1U << 1),
    CONNECTIONS_ALL = CONNECTIONS_IN | CONNECTIONS_OUT,
};

enum BlockSource {
    BLOCK_SOURCE_NONE,
    BLOCK_SOURCE_REINDEX,
    BLOCK_SOURCE_DISK,
    BLOCK_SOURCE_NETWORK
};

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

class ClientModel : public QObject
{
    Q_OBJECT

public:
    explicit ClientModel(OptionsModel *optionsModel, QObject *parent = 0);
    ~ClientModel();

    OptionsModel *getOptionsModel();
    PeerTableModel *getPeerTableModel();
    BanTableModel *getBanTableModel();

    //! Return number of connections, default is in- and outbound (total)
    int getNumConnections(unsigned int flags = CONNECTIONS_ALL) const;
    int getNumBlocks() const;
    int getHeaderTipHeight() const;
    int64_t getHeaderTipTime() const;
    quint64 getTotalBytesRecv() const;
    quint64 getTotalBytesSent() const;
    QDateTime getLastBlockDate() const;
    long getMempoolSize() const;
    size_t getMempoolDynamicUsage() const;
    double getVerificationProgress(const CBlockIndex *tipIn = 0) const;

    enum BlockSource getBlockSource() const;

    //! Return network name
    QString getNetworkName() const;

    //! Return true if core is doing initial block download
    bool inInitialBlockDownload() const;

    QString getStatusBarWarnings() const;
    QString formatFullVersion() const;
    QString formatSubVersion() const;
    bool isReleaseVersion() const;
    QString formatClientStartupTime() const;
    QString dataDir() const;

    //! Return cached block height to avoid blocking GUI during sync
    int getCachedNumBlocks() const { return cachedNumBlocks; }
    int getCachedHeaderTipHeight() const { return cachedHeaderTipHeight; }

private:
    OptionsModel *optionsModel;
    PeerTableModel *peerTableModel;
    BanTableModel *banTableModel;
    QTimer *pollTimer;

    //! Cached values to return when cs_main is contended during sync
    mutable int cachedNumConnections;
    mutable int cachedNumBlocks;
    mutable int cachedHeaderTipHeight;
    mutable int64_t cachedHeaderTipTime;
    mutable QDateTime cachedLastBlockDate;
    mutable double cachedVerificationProgress;
    mutable bool cachedInitialBlockDownload;

    void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();

public Q_SLOTS:
    void updateTimer();
    void updateNumConnections(int numConnections);
    void updateAlert();
    void updateBanlist();

Q_SIGNALS:
    void numConnectionsChanged(int count);
    void numBlocksChanged(int count, const QDateTime& blockDate, double nVerificationProgress, bool header);
    void alertsChanged(const QString &warnings);
    void bytesChanged(quint64 totalBytesIn, quint64 totalBytesOut);
    void mempoolSizeChanged(long count, size_t mempoolSizeInBytes);
    void networkActiveChanged(bool networkActive);

    //! Show progress dialog e.g. for verifychain
    void showProgress(const QString &title, int nProgress);

    friend class BitcoinCore;
};

#endif // BITCOIN_QT_CLIENTMODEL_H
