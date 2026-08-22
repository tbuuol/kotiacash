// Copyright (c) 2024 Kotia developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_COINJOIN_H
#define BITCOIN_COINJOIN_H

#include "amount.h"
#include "primitives/transaction.h"
#include "pubkey.h"
#include "sync.h"
#include "utilstrencodings.h"
#include "utiltime.h"

#include <atomic>
#include <map>
#include <set>
#include <string>
#include <vector>

class COutput;
class CWallet;
class CWalletTx;

/**
 * CoinJoin implementation for Kotia
 * Based on Dash PrivateSend / Wasabi Wallet model
 *
 * Architecture:
 * - Client-side mixing with optional coordinator
 * - Multiple participants create a joint transaction
 * - All inputs are equal denomination, all outputs are equal denomination
 * - Breaks the link between sender and receiver
 *
 * Denominations: 0.001, 0.01, 0.1, 1, 10 KOT
 */

/** CoinJoin denomination levels (in satoshi) */
static const CAmount COINJOIN_DENOM_1 = 100000;        // 0.001 KOT
static const CAmount COINJOIN_DENOM_2 = 1000000;       // 0.01 KOT
static const CAmount COINJOIN_DENOM_3 = 10000000;      // 0.1 KOT
static const CAmount COINJOIN_DENOM_4 = 100000000;     // 1 KOT
static const CAmount COINJOIN_DENOM_5 = 1000000000;    // 10 KOT

/** Default CoinJoin fee (per denomination) */
static const CAmount COINJOIN_DEFAULT_FEE = 10000; // 0.0001 KOT

/** Minimum participants for a CoinJoin session */
static const int COINJOIN_MIN_PARTICIPANTS = 2;

/** Maximum participants for a CoinJoin session */
static const int COINJOIN_MAX_PARTICIPANTS = 10;

/** Timeout for CoinJoin session (seconds) */
static const int64_t COINJOIN_SESSION_TIMEOUT = 300;

/** Minimum rounds of mixing for privacy */
static const int COINJOIN_DEFAULT_ROUNDS = 4;

/** Maximum inputs per CoinJoin transaction to stay within mempool limits.
 *  ~20 inputs × ~150 bytes each ≈ 3 KB, well under DEFAULT_MAX_TX_SIZE. */
static const int COINJOIN_MAX_INPUTS = 20;

/** Max unconfirmed CoinJoin txs in a chain before waiting for confirmations.
 *  Mempool DEFAULT_DESCENDANT_LIMIT is 25; we stop at 20 to leave headroom. */
static const int COINJOIN_MAX_UNCONFIRMED_CHAIN = 20;

/** Minimum satoshi value of any mixed output.
 *  Must stay above the dust threshold: with the fork's raised
 *  minRelayTxFee (10000 sat/kB) a P2PKH output needs >= 5460 satoshi,
 *  so 10000 is a safe rounding step. */
static const CAmount COINJOIN_MIN_MIX_OUTPUT = 10000;

/** CoinJoin session states */
enum CoinJoinState {
    CJ_STATE_IDLE,
    CJ_STATE_WAITING_FOR_PARTICIPANTS,
    CJ_STATE_INPUTS_REGISTERED,
    CJ_STATE_OUTPUTS_SUBMITTED,
    CJ_STATE_SIGNING,
    CJ_STATE_BROADCAST,
    CJ_STATE_COMPLETE,
    CJ_STATE_ERROR
};

/** CoinJoin participant information */
struct CoinJoinParticipant {
    CPubKey pubKey;
    std::vector<COutPoint> inputs;
    std::vector<CTxOut> outputs;
    bool inputsSigned;
    bool outputsSubmitted;

    CoinJoinParticipant() : inputsSigned(false), outputsSubmitted(false) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(pubKey);
        READWRITE(inputs);
        READWRITE(outputs);
        READWRITE(inputsSigned);
        READWRITE(outputsSubmitted);
    }
};

/** CoinJoin session */
class CoinJoinSession {
public:
    uint256 sessionId;
    CAmount denomination;        // The denomination being mixed
    int nRounds;                 // Number of mixing rounds completed
    int nMinParticipants;
    int nMaxParticipants;
    int64_t nTimeCreated;
    int64_t nTimeLastActivity;
    CoinJoinState state;
    std::map<uint256, CoinJoinParticipant> participants;
    CMutableTransaction tx;      // The joint transaction
    std::string strError;

    CoinJoinSession() {
        denomination = COINJOIN_DENOM_2;
        nRounds = 0;
        nMinParticipants = COINJOIN_MIN_PARTICIPANTS;
        nMaxParticipants = COINJOIN_MAX_PARTICIPANTS;
        nTimeCreated = 0;
        nTimeLastActivity = 0;
        state = CJ_STATE_IDLE;
    }

    bool IsExpired() const {
        return nTimeLastActivity > 0 &&
               (GetTime() - nTimeLastActivity) > COINJOIN_SESSION_TIMEOUT;
    }

    int GetParticipantCount() const {
        return participants.size();
    }

    bool AddParticipant(const uint256& participantId, const CoinJoinParticipant& participant) {
        if ((int)participants.size() >= nMaxParticipants)
            return false;
        participants[participantId] = participant;
        nTimeLastActivity = GetTime();
        return true;
    }

    bool RemoveParticipant(const uint256& participantId) {
        return participants.erase(participantId) > 0;
    }

    bool AllInputsRegistered() const {
        for (const auto& pair : participants) {
            if (!pair.second.inputsSigned)
                return false;
        }
        return true;
    }

    bool AllOutputsSubmitted() const {
        for (const auto& pair : participants) {
            if (!pair.second.outputsSubmitted)
                return false;
        }
        return true;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(sessionId);
        READWRITE(denomination);
        READWRITE(nRounds);
        READWRITE(nMinParticipants);
        READWRITE(nMaxParticipants);
        READWRITE(nTimeCreated);
        READWRITE(nTimeLastActivity);
        int32_t nState = static_cast<int32_t>(state);
        READWRITE(nState);
        if (ser_action.ForRead())
            state = static_cast<CoinJoinState>(nState);
        READWRITE(participants);
    }
};

/** CoinJoin configuration */
struct CoinJoinConfig {
    CAmount nDenom;              // Target denomination
    int nRounds;                 // Number of mixing rounds
    int nSessions;               // Number of parallel sessions
    bool fEnable;                // Auto-mix enabled
    CAmount nMaxFee;             // Maximum fee willing to pay

    CoinJoinConfig() {
        nDenom = COINJOIN_DENOM_2;
        nRounds = COINJOIN_DEFAULT_ROUNDS;
        nSessions = 3;
        fEnable = false;
        nMaxFee = COINJOIN_DEFAULT_FEE * 10;
    }
};

/** CoinJoin collateral input for DoS protection */
struct CoinJoinCollateral {
    COutPoint outpoint;
    CAmount amount;
    bool IsExpired() const;
};

/**
 * CoinJoin Manager - handles all mixing operations
 *
 * Thread-safety: all shared state is guarded by cs_coinjoin. Public
 * getters return copies. Mixing itself (AutoMix) runs without holding
 * cs_coinjoin so that stop requests and status queries remain
 * responsive while a background worker mixes.
 *
 * Lock ordering: never hold cs_coinjoin while acquiring cs_main or
 * cs_wallet (callers such as the RPC layer commonly hold those first).
 */
class CCoinJoin {
private:
    mutable CCriticalSection cs_coinjoin;

    std::map<uint256, CoinJoinSession> mapSessions;
    CoinJoinConfig config;

    // Track mixed outputs
    std::set<COutPoint> setMixedOutpoints;
    std::map<COutPoint, int> mapOutpointRounds; // Track rounds per output

    // Collateral tracking
    std::vector<CoinJoinCollateral> vecCollateral;

    // Session statistics
    int64_t nTotalMixed;
    int64_t nTotalSessions;
    int64_t nTotalFees;

    // Mixing state (fMixing is atomic for cheap lock-free polling)
    std::atomic<bool> fMixing;
    std::atomic<bool> fStopRequested;
    int nLastRoundsRequested;
    int nLastRoundsCompleted;
    int nLastTransactionsRequested;
    int nLastTransactionsCompleted;
    std::string strLastStatus;

    /** Update status string (cs_coinjoin must NOT be held by caller) */
    void SetStatus(const std::string& status);

    /** Internal unlocked helpers (cs_coinjoin must be held) */
    int GetOutpointRoundsUnlocked(const COutPoint& outpoint) const;
    void MarkMixedUnlocked(const COutPoint& outpoint, int rounds);

public:
    CCoinJoin();

    /** Initialize CoinJoin system */
    bool Init();

    /** Get/set configuration */
    CoinJoinConfig GetConfig() const {
        LOCK(cs_coinjoin);
        return config;
    }
    void SetConfig(const CoinJoinConfig& cfg) {
        LOCK(cs_coinjoin);
        config = cfg;
    }

    /**
     * Create a new CoinJoin session
     * @param denomination The denomination to mix
     * @param sessionId Output: created session ID
     * @return true on success
     */
    bool CreateSession(CAmount denomination, uint256& sessionId);

    /**
     * Join an existing CoinJoin session
     * @param sessionId Session to join
     * @param inputs Inputs to contribute
     * @param outputs Outputs to receive
     * @return true on success
     */
    bool JoinSession(const uint256& sessionId, const std::vector<COutPoint>& inputs,
                     const std::vector<CTxOut>& outputs);

    /**
     * Submit signed inputs for a session
     * @param sessionId Session ID
     * @param participantId Participant ID
     * @param txSigned Signed transaction
     * @return true on success
     */
    bool SubmitSignedInput(const uint256& sessionId, const uint256& participantId,
                           const CTransaction& txSigned);

    /**
     * Submit outputs for a session
     * @param sessionId Session ID
     * @param participantId Participant ID
     * @param outputs Outputs to receive
     * @return true on success
     */
    bool SubmitOutputs(const uint256& sessionId, const uint256& participantId,
                       const std::vector<CTxOut>& outputs);

    /**
     * Sign and broadcast a completed session
     * @param sessionId Session ID
     * @param wallet Wallet to sign with
     * @return true on success
     */
    bool SignAndBroadcast(const uint256& sessionId, CWallet* wallet);

    /**
     * Get available denominations for mixing
     * @return Vector of denomination amounts
     */
    std::vector<CAmount> GetDenominations() const;

    /**
     * Check if an amount is a valid denomination
     */
    bool IsValidDenomination(CAmount amount) const;

    /**
     * Find suitable inputs for a denomination from wallet
     * @param wallet Wallet to search
     * @param denomination Target denomination
     * @param nRoundsRequired Minimum rounds the inputs should have
     * @param outputs Output: selected outputs
     * @return true if suitable inputs found
     */
    bool SelectCoinsForDenomination(CWallet* wallet, CAmount denomination,
                                     int nRoundsRequired, std::vector<COutput>& outputs);

    /**
     * Split an amount into randomized mixed output amounts.
     *
     * Invariants (asserted by unit tests):
     *  - the amounts sum to exactly amountToMix
     *  - every amount is >= COINJOIN_MIN_MIX_OUTPUT
     *  - every amount is <= maxOutputAmount
     *  - at least 2 outputs when amountToMix allows it
     * Pure function: safe to call from tests without any wallet or chain.
     *
     * @return empty vector if the amount cannot be split
     */
    static std::vector<CAmount> SplitAmountsForMixing(CAmount amountToMix, CAmount maxOutputAmount);

    /**
     * Create a CoinJoin transaction (client-side, no coordinator)
     * @param wallet Wallet
     * @param denomination Target denomination
     * @param wtxNew Output: created transaction
     * @param nFeeRet Output: fee
     * @param allowUnconfirmed Allow using unconfirmed coins (for multi-round mixing)
     * @param mixAmount Amount to mix (0 = use all available)
     * @return true on success
     */
    bool CreateCoinJoinTransaction(CWallet* wallet, CAmount denomination,
                                     CWalletTx& wtxNew, CAmount& nFeeRet, bool allowUnconfirmed = false, CAmount mixAmount = 0);

    /**
     * Auto-mix wallet balance to target denomination
     * @param wallet Wallet
     * @param denomination Target denomination
     * @param rounds Number of rounds
     * @param mixAmount Amount to mix (0 = use all available)
     * @return Number of mixing transactions created
     */
    int AutoMix(CWallet* wallet, CAmount denomination, int rounds, CAmount mixAmount = 0);

    /**
     * Stop ongoing mixing process
     */
    void StopMixing();

    /**
     * Check if mixing is currently active
     */
    bool IsMixing() const { return fMixing.load(); }

    /**
     * Get mixing progress for a denomination
     */
    double GetMixingProgress(CWallet* wallet, CAmount denomination);

    /**
     * Get the largest suitable denomination for available coins
     * @param wallet Wallet
     * @param allowUnconfirmed Include unconfirmed coins
     * @return Largest denomination the user can mix, or 0 if no coins
     */
    CAmount SuggestDenomination(CWallet* wallet, bool allowUnconfirmed = false) const;

    /**
     * Get available balance info for CoinJoin
     * @param wallet Wallet
     * @param allowUnconfirmed Include unconfirmed coins
     * @return Pair of (total balance, number of coins)
     */
    std::pair<CAmount, int> GetAvailableBalance(CWallet* wallet, bool allowUnconfirmed = false) const;

    /**
     * Get number of rounds for an outpoint
     */
    int GetOutpointRounds(const COutPoint& outpoint) const {
        LOCK(cs_coinjoin);
        return GetOutpointRoundsUnlocked(outpoint);
    }

    /**
     * Mark an outpoint as mixed
     */
    void MarkMixed(const COutPoint& outpoint, int rounds = 1) {
        LOCK(cs_coinjoin);
        MarkMixedUnlocked(outpoint, rounds);
    }

    /**
     * Get session info
     */
    CoinJoinSession GetSession(const uint256& sessionId) const;

    /**
     * Get all active sessions
     */
    std::vector<CoinJoinSession> GetActiveSessions() const;

    /**
     * Cancel a session
     */
    bool CancelSession(const uint256& sessionId);

    /**
     * Clean up expired sessions
     */
    void CleanupExpiredSessions();

    /**
     * Get statistics
     */
    int64_t GetTotalMixed() const {
        LOCK(cs_coinjoin);
        return nTotalMixed;
    }
    int64_t GetTotalSessions() const {
        LOCK(cs_coinjoin);
        return nTotalSessions;
    }
    int64_t GetTotalFees() const {
        LOCK(cs_coinjoin);
        return nTotalFees;
    }
    int GetLastRoundsRequested() const {
        LOCK(cs_coinjoin);
        return nLastRoundsRequested;
    }
    int GetLastRoundsCompleted() const {
        LOCK(cs_coinjoin);
        return nLastRoundsCompleted;
    }
    int GetLastTransactionsRequested() const {
        LOCK(cs_coinjoin);
        return nLastTransactionsRequested;
    }
    int GetLastTransactionsCompleted() const {
        LOCK(cs_coinjoin);
        return nLastTransactionsCompleted;
    }
    /** Thread-safe status copy for cross-thread UI/RPC readers */
    std::string GetLastStatus() const {
        LOCK(cs_coinjoin);
        return strLastStatus;
    }

    /**
     * Create collateral for DoS protection
     */
    bool CreateCollateral(CWallet* wallet);

    /**
     * Check if we have valid collateral
     */
    bool HasCollateral() const;
};

/** Global CoinJoin instance */
extern CCoinJoin* pcoinjoin;

/** Helper: format denomination amount */
std::string FormatDenomination(CAmount amount);

/** Helper: get standard denominations */
std::vector<CAmount> GetStandardDenominations();

#endif // BITCOIN_COINJOIN_H
