// Copyright (c) 2024 Kotia developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_COINJOINWORKER_H
#define BITCOIN_QT_COINJOINWORKER_H

#include "amount.h"
#include <QObject>
#include <QMutex>
#include <QAtomicInt>

class CWallet;

/**
 * Background worker for CoinJoin mixing operations.
 * Runs AutoMix in a separate thread to prevent UI freezing.
 * Communicates status updates via Qt signals.
 */
class CoinJoinWorker : public QObject
{
    Q_OBJECT

public:
    explicit CoinJoinWorker(CWallet* wallet, QObject* parent = nullptr);

    /** Start mixing with given parameters (runs in worker thread) */
    void startMixing(CAmount denomination, int rounds, CAmount mixAmount);

    /** Request stop (thread-safe) */
    void requestStop();

    /** Check if currently mixing */
    bool isMixing() const;

Q_SIGNALS:
    /** Emitted when mixing status changes */
    void statusChanged(const QString& status);

    /** Emitted when mixing completes */
    void mixingFinished(int transactionsCreated, int transactionsRequested);

    /** Emitted when partial completion occurs */
    void mixingPartial(int transactionsCreated, int transactionsRequested, const QString& status);

    /** Emitted when mixing fails */
    void mixingFailed(const QString& error);

public Q_SLOTS:
    /** Worker slot - runs in background thread */
    void doWork();

private:
    CWallet* m_wallet;
    CAmount m_denomination;
    int m_rounds;
    CAmount m_mixAmount;

    mutable QMutex m_mutex;
    QAtomicInt m_stopRequested;
    QAtomicInt m_isMixing;
};

#endif // BITCOIN_QT_COINJOINWORKER_H
