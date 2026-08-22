// Copyright (c) 2024 Kotia developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/coinjoinworker.h"
#include "wallet/wallet.h"
#include "util.h"


CoinJoinWorker::CoinJoinWorker(CWallet* wallet, QObject* parent)
    : QObject(parent)
    , m_wallet(wallet)
    , m_denomination(0)
    , m_rounds(0)
    , m_mixAmount(0)
    , m_stopRequested(0)
    , m_isMixing(0)
{
}

void CoinJoinWorker::startMixing(CAmount denomination, int rounds, CAmount mixAmount)
{
    QMutexLocker lock(&m_mutex);
    m_denomination = denomination;
    m_rounds = rounds;
    m_mixAmount = mixAmount;
    m_stopRequested = 0;
    m_isMixing = 1;

    // Queue doWork() to run in the worker thread
    QMetaObject::invokeMethod(this, "doWork", Qt::QueuedConnection);
}

void CoinJoinWorker::requestStop()
{
    m_stopRequested = 1;
    if (m_wallet) {
        m_wallet->coinJoin.StopMixing();
    }
}

bool CoinJoinWorker::isMixing() const
{
    return m_isMixing == 1;
}

void CoinJoinWorker::doWork()
{
    if (!m_wallet) {
        Q_EMIT mixingFailed("No wallet available");
        m_isMixing = 0;
        return;
    }

    // NOTE: there is deliberately no status-polling QTimer here: doWork()
    // blocks this thread's event loop, so a timer would never fire during
    // mixing. The CoinJoinPage already polls GetLastStatus() from the GUI
    // thread, and CCoinJoin::GetLastStatus() is thread-safe (returns a copy
    // under cs_coinjoin).

    // Run the blocking AutoMix call
    int created = m_wallet->AutoMixCoins(m_denomination, m_rounds, m_mixAmount);

    m_isMixing = 0;

    // Get final status
    QString finalStatus = QString::fromStdString(m_wallet->coinJoin.GetLastStatus());
    int requested = m_wallet->coinJoin.GetLastTransactionsRequested();

    if (m_stopRequested) {
        Q_EMIT mixingFinished(created, requested);
    } else if (created == requested && requested > 0) {
        Q_EMIT mixingFinished(created, requested);
    } else if (created > 0) {
        Q_EMIT mixingPartial(created, requested, finalStatus);
    } else {
        Q_EMIT mixingFailed(finalStatus.isEmpty() ? "Mixing failed" : finalStatus);
    }
}
