// Copyright (c) 2024 Kotia developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "coinjoin.h"

#include "base58.h"
#include "consensus/consensus.h"
#include "core_io.h"
#include "key.h"
#include "main.h"
#include "net.h"
#include "random.h"
#include "script/sign.h"
#include "timedata.h"
#include "txmempool.h"
#include "util.h"
#include "utilmoneystr.h"
#include "validationinterface.h"
#include "wallet/wallet.h"

#include <boost/thread.hpp>

CCoinJoin* pcoinjoin = NULL;

CCoinJoin::CCoinJoin()
{
    nTotalMixed = 0;
    nTotalSessions = 0;
    nTotalFees = 0;
    fMixing = false;
    fStopRequested = false;
    nLastRoundsRequested = 0;
    nLastRoundsCompleted = 0;
    nLastTransactionsRequested = 0;
    nLastTransactionsCompleted = 0;
    strLastStatus = "CoinJoin idle";
}

void CCoinJoin::SetStatus(const std::string& status)
{
    LOCK(cs_coinjoin);
    strLastStatus = status;
}

bool CCoinJoin::Init()
{
    LOCK(cs_coinjoin);
    LogPrintf("CoinJoin: Initializing...\n");
    // Purge stale sessions left over from a previous run.
    std::vector<uint256> toRemove;
    for (const auto& pair : mapSessions) {
        if (pair.second.IsExpired() || pair.second.state == CJ_STATE_COMPLETE ||
            pair.second.state == CJ_STATE_ERROR) {
            toRemove.push_back(pair.first);
        }
    }
    for (const auto& id : toRemove)
        mapSessions.erase(id);
    if (!toRemove.empty())
        LogPrintf("CoinJoin: Cleaned up %d expired sessions\n", (int)toRemove.size());
    return true;
}

bool CCoinJoin::CreateSession(CAmount denomination, uint256& sessionId)
{
    if (!IsValidDenomination(denomination)) {
        LogPrintf("CoinJoin: Invalid denomination %s\n", FormatMoney(denomination));
        return false;
    }

    CoinJoinSession session;
    session.sessionId = GetRandHash();
    session.denomination = denomination;
    session.nTimeCreated = GetTime();
    session.nTimeLastActivity = GetTime();
    session.state = CJ_STATE_WAITING_FOR_PARTICIPANTS;

    LOCK(cs_coinjoin);
    sessionId = session.sessionId;
    mapSessions[sessionId] = session;

    LogPrintf("CoinJoin: Created session %s for denomination %s\n",
              sessionId.ToString().substr(0, 8), FormatMoney(denomination));
    return true;
}

bool CCoinJoin::JoinSession(const uint256& sessionId, const std::vector<COutPoint>& inputs,
                             const std::vector<CTxOut>& outputs)
{
    LOCK(cs_coinjoin);

    auto it = mapSessions.find(sessionId);
    if (it == mapSessions.end()) {
        LogPrintf("CoinJoin: Session %s not found\n", sessionId.ToString().substr(0, 8));
        return false;
    }

    CoinJoinSession& session = it->second;

    if (session.IsExpired()) {
        LogPrintf("CoinJoin: Session %s expired\n", sessionId.ToString().substr(0, 8));
        return false;
    }

    if (session.state != CJ_STATE_WAITING_FOR_PARTICIPANTS &&
        session.state != CJ_STATE_INPUTS_REGISTERED) {
        LogPrintf("CoinJoin: Session %s not accepting participants\n", sessionId.ToString().substr(0, 8));
        return false;
    }

    if (session.GetParticipantCount() >= session.nMaxParticipants) {
        LogPrintf("CoinJoin: Session %s full\n", sessionId.ToString().substr(0, 8));
        return false;
    }

    CoinJoinParticipant participant;
    participant.inputs = inputs;
    participant.outputs = outputs;

    uint256 participantId = GetRandHash();
    session.AddParticipant(participantId, participant);

    // Update session state
    if (session.GetParticipantCount() >= session.nMinParticipants) {
        session.state = CJ_STATE_INPUTS_REGISTERED;
    }

    LogPrintf("CoinJoin: Participant %s joined session %s (%d/%d)\n",
              participantId.ToString().substr(0, 8),
              sessionId.ToString().substr(0, 8),
              session.GetParticipantCount(), session.nMinParticipants);

    return true;
}

bool CCoinJoin::SubmitSignedInput(const uint256& sessionId, const uint256& participantId,
                                   const CTransaction& txSigned)
{
    LOCK(cs_coinjoin);

    auto it = mapSessions.find(sessionId);
    if (it == mapSessions.end())
        return false;

    CoinJoinSession& session = it->second;
    auto pit = session.participants.find(participantId);
    if (pit == session.participants.end())
        return false;

    pit->second.inputsSigned = true;
    session.nTimeLastActivity = GetTime();

    // Check if all inputs are registered
    if (session.AllInputsRegistered()) {
        session.state = CJ_STATE_OUTPUTS_SUBMITTED;
    }

    return true;
}

bool CCoinJoin::SubmitOutputs(const uint256& sessionId, const uint256& participantId,
                               const std::vector<CTxOut>& outputs)
{
    LOCK(cs_coinjoin);

    auto it = mapSessions.find(sessionId);
    if (it == mapSessions.end())
        return false;

    CoinJoinSession& session = it->second;
    auto pit = session.participants.find(participantId);
    if (pit == session.participants.end())
        return false;

    // Validate outputs match denomination
    for (const auto& out : outputs) {
        if (out.nValue != session.denomination) {
            LogPrintf("CoinJoin: Output value mismatch in session %s\n",
                      sessionId.ToString().substr(0, 8));
            return false;
        }
    }

    pit->second.outputs = outputs;
    pit->second.outputsSubmitted = true;
    session.nTimeLastActivity = GetTime();

    if (session.AllOutputsSubmitted()) {
        session.state = CJ_STATE_SIGNING;
    }

    return true;
}

bool CCoinJoin::SignAndBroadcast(const uint256& sessionId, CWallet* wallet)
{
    // Copy the session and drop the lock before touching the wallet or the
    // mempool: cs_main/cs_wallet must never be acquired while holding
    // cs_coinjoin (lock-order inversion with the RPC layer).
    CoinJoinSession session;
    {
        LOCK(cs_coinjoin);
        auto it = mapSessions.find(sessionId);
        if (it == mapSessions.end())
            return false;
        session = it->second;
    }

    if (session.state != CJ_STATE_SIGNING) {
        LogPrintf("CoinJoin: Session %s not ready for signing (state=%d)\n",
                  sessionId.ToString().substr(0, 8), session.state);
        return false;
    }

    // Build the joint transaction
    CMutableTransaction tx;
    tx.nVersion = 1; // Version 2 rejected if CSV not active
    tx.nTime = GetAdjustedTime(); // Must be >= input transaction times
    tx.nLockTime = 0;

    CAmount totalInput = 0;
    CAmount totalOutput = 0;

    // Add all inputs
    for (const auto& pair : session.participants) {
        const CoinJoinParticipant& p = pair.second;
        for (const auto& outpoint : p.inputs) {
            CTxIn in(outpoint);
            tx.vin.push_back(in);

            // Get input amount from wallet
            LOCK(wallet->cs_wallet);
            auto wit = wallet->mapWallet.find(outpoint.hash);
            if (wit != wallet->mapWallet.end() && outpoint.n < wit->second.vout.size()) {
                totalInput += wit->second.vout[outpoint.n].nValue;
            }
        }
    }

    // Add all outputs
    for (const auto& pair : session.participants) {
        const CoinJoinParticipant& p = pair.second;
        for (const auto& out : p.outputs) {
            tx.vout.push_back(out);
            totalOutput += out.nValue;
        }
    }

    // Fee is the difference
    CAmount nFee = totalInput - totalOutput;
    if (nFee < 0) {
        LogPrintf("CoinJoin: Negative fee in session %s\n", sessionId.ToString().substr(0, 8));
        LOCK(cs_coinjoin);
        auto it = mapSessions.find(sessionId);
        if (it != mapSessions.end()) {
            it->second.state = CJ_STATE_ERROR;
            it->second.strError = "Negative fee";
        }
        return false;
    }

    // Snapshot input scripts under the wallet lock, then sign unlocked.
    std::vector<std::pair<CScript, CAmount> > prevouts;
    {
        LOCK(wallet->cs_wallet);
        for (unsigned int i = 0; i < tx.vin.size(); i++) {
            const COutPoint& outpoint = tx.vin[i].prevout;
            auto wit = wallet->mapWallet.find(outpoint.hash);
            if (wit == wallet->mapWallet.end() || outpoint.n >= wit->second.vout.size()) {
                prevouts.push_back(std::make_pair(CScript(), 0));
                continue;
            }
            prevouts.push_back(std::make_pair(wit->second.vout[outpoint.n].scriptPubKey,
                                              wit->second.vout[outpoint.n].nValue));
        }
    }

    for (unsigned int i = 0; i < tx.vin.size(); i++) {
        if (prevouts[i].first.empty())
            continue;
        SignatureData sigdata;
        ProduceSignature(
            MutableTransactionSignatureCreator(wallet, &tx, i, prevouts[i].second, SIGHASH_ALL),
            prevouts[i].first, sigdata);
        UpdateTransaction(tx, i, sigdata);
    }

    // Broadcast
    CTransaction txFinal(tx);
    CValidationState state;
    {
        LOCK(cs_main);
        if (!AcceptToMemoryPool(mempool, state, txFinal, false, NULL)) {
            LogPrintf("CoinJoin: Failed to accept transaction to mempool: %s\n",
                      state.GetRejectReason());
            LOCK(cs_coinjoin);
            auto it = mapSessions.find(sessionId);
            if (it != mapSessions.end()) {
                it->second.state = CJ_STATE_ERROR;
                it->second.strError = state.GetRejectReason();
            }
            return false;
        }
    }

    RelayTransaction(txFinal);

    // Mark outputs as mixed and update stats
    {
        LOCK(cs_coinjoin);
        for (unsigned int i = 0; i < tx.vout.size(); i++) {
            COutPoint outpoint(txFinal.GetHash(), i);
            MarkMixedUnlocked(outpoint, session.nRounds + 1);
        }
        nTotalMixed += totalOutput;
        nTotalSessions++;
        nTotalFees += nFee;

        auto it = mapSessions.find(sessionId);
        if (it != mapSessions.end()) {
            it->second.state = CJ_STATE_COMPLETE;
            it->second.nRounds++;
        }
    }

    LogPrintf("CoinJoin: Session %s completed - %s mixed, fee: %s\n",
              sessionId.ToString().substr(0, 8),
              FormatMoney(totalOutput), FormatMoney(nFee));

    return true;
}

std::vector<CAmount> CCoinJoin::GetDenominations() const
{
    return GetStandardDenominations();
}

bool CCoinJoin::IsValidDenomination(CAmount amount) const
{
    std::vector<CAmount> denoms = GetStandardDenominations();
    for (const auto& d : denoms) {
        if (d == amount)
            return true;
    }
    return false;
}

bool CCoinJoin::SelectCoinsForDenomination(CWallet* wallet, CAmount denomination,
                                             int nRoundsRequired, std::vector<COutput>& outputs)
{
    LOCK2(cs_main, wallet->cs_wallet);

    std::vector<COutput> available;
    wallet->AvailableCoins(available, true, NULL, false);

    // Do NOT hold cs_coinjoin across the loop: GetOutpointRounds() takes it.
    for (const auto& out : available) {
        if (out.tx->vout[out.i].nValue != denomination)
            continue;

        // Check if already mixed enough rounds
        COutPoint outpoint(out.tx->GetHash(), out.i);
        int rounds = GetOutpointRounds(outpoint);
        if (rounds >= nRoundsRequired)
            continue;

        outputs.push_back(out);
    }

    return !outputs.empty();
}

std::vector<CAmount> CCoinJoin::SplitAmountsForMixing(CAmount amountToMix, CAmount maxOutputAmount)
{
    std::vector<CAmount> outputAmounts;
    if (amountToMix <= 0 || maxOutputAmount < COINJOIN_MIN_MIX_OUTPUT)
        return outputAmounts;
    if (amountToMix < 2 * COINJOIN_MIN_MIX_OUTPUT)
        return outputAmounts;

    // Base number of outputs, plus 0-2 extra pieces for amount variety.
    int minOutputs = std::max(2, (int)((amountToMix + maxOutputAmount - 1) / maxOutputAmount));
    int extraOutputs = GetRandInt(3);
    int nOutputs = minOutputs + extraOutputs;

    // Never more outputs than the minimum-mix amount allows.
    while (nOutputs > 2 && amountToMix / nOutputs < COINJOIN_MIN_MIX_OUTPUT)
        nOutputs--;

    outputAmounts.reserve(nOutputs);
    CAmount remaining = amountToMix;

    for (int i = 0; i < nOutputs; i++) {
        CAmount amount;
        if (i == nOutputs - 1) {
            amount = remaining;
        } else {
            int outputsLeft = nOutputs - i;
            CAmount average = remaining / outputsLeft;
            CAmount maxAllowed = std::min(maxOutputAmount, remaining - (outputsLeft - 1) * COINJOIN_MIN_MIX_OUTPUT);
            // The tail must still fit into the remaining outputs: never take
            // so much that the last outputs would exceed maxOutputAmount.
            CAmount tailBound = remaining - (outputsLeft - 1) * maxOutputAmount;
            CAmount minAllowed = std::max(std::max(COINJOIN_MIN_MIX_OUTPUT, average * 50 / 100), tailBound);
            if (minAllowed > maxAllowed)
                minAllowed = maxAllowed;
            CAmount span = maxAllowed > minAllowed ? maxAllowed - minAllowed : 0;
            amount = minAllowed + (span > 0 ? GetRandInt(span + 1) : 0);
            // Round to a clean step for nicer-looking amounts, then re-clamp
            // (bounds may not be multiples of the step).
            amount = ((amount + COINJOIN_MIN_MIX_OUTPUT / 2) / COINJOIN_MIN_MIX_OUTPUT) * COINJOIN_MIN_MIX_OUTPUT;
            if (amount < minAllowed) amount = minAllowed;
            if (amount > maxAllowed) amount = maxAllowed;
        }
        outputAmounts.push_back(amount);
        remaining -= amount;
    }

    // Repair pass: enforce exact total while respecting the bounds.
    CAmount outputTotal = 0;
    for (CAmount a : outputAmounts) outputTotal += a;
    if (outputTotal != amountToMix) {
        CAmount delta = amountToMix - outputTotal;
        for (size_t i = 0; i < outputAmounts.size() && delta != 0; i++) {
            if (delta > 0) {
                CAmount room = maxOutputAmount - outputAmounts[i];
                CAmount add = std::min(room, delta);
                outputAmounts[i] += add;
                delta -= add;
            } else {
                CAmount removable = outputAmounts[i] - COINJOIN_MIN_MIX_OUTPUT;
                CAmount remove = std::min(removable, -delta);
                outputAmounts[i] -= remove;
                delta += remove;
            }
        }
        if (delta != 0)
            return std::vector<CAmount>(); // cannot distribute exactly
    }

    return outputAmounts;
}

bool CCoinJoin::CreateCoinJoinTransaction(CWallet* wallet, CAmount denomination,
                                           CWalletTx& wtxNew, CAmount& nFeeRet, bool allowUnconfirmed, CAmount mixAmount)
{
    nFeeRet = 0;

    // Basic parameter validation (guards div-by-zero downstream).
    if (denomination < COINJOIN_MIN_MIX_OUTPUT + COINJOIN_DEFAULT_FEE) {
        LogPrintf("CoinJoin: Denomination %s too small\n", FormatMoney(denomination));
        return false;
    }
    if (mixAmount < 0) {
        LogPrintf("CoinJoin: Negative mix amount %s\n", FormatMoney(mixAmount));
        return false;
    }

    // Select coins
    std::vector<COutput> vCoins;
    wallet->AvailableCoins(vCoins, !allowUnconfirmed, NULL, false);

    CAmount totalValue = 0;
    CAmount totalWalletValue = 0;
    int nCoinsAvailable = 0;

    // First pass: analyze available coins
    for (const auto& out : vCoins) {
        totalWalletValue += out.tx->vout[out.i].nValue;
        nCoinsAvailable++;
    }

    LogPrintf("CoinJoin: Analyzing coins - total: %s, available: %d, denom: %s, allowUnconf: %d, mixAmount: %s\n",
              FormatMoney(totalWalletValue), nCoinsAvailable,
              FormatMoney(denomination), allowUnconfirmed, FormatMoney(mixAmount));

    if (nCoinsAvailable == 0) {
        LogPrintf("CoinJoin: No coins available at all\n");
        return false;
    }

    // Rough fee estimate for coin selection break condition.
    // Will be recalculated precisely after inputs are selected.
    CAmount fee = std::max(COINJOIN_DEFAULT_FEE, ::minRelayTxFee.GetFee(3000));

    // Determine target input amount
    CAmount targetInput = mixAmount > 0 ? mixAmount : totalWalletValue;

    // Prefer already-mixed outputs in later rounds so each requested round
    // remixes the prior round instead of starting over from unrelated change.
    if (allowUnconfirmed) {
        std::sort(vCoins.begin(), vCoins.end(), [this](const COutput& a, const COutput& b) {
            int ra = GetOutpointRounds(COutPoint(a.tx->GetHash(), a.i));
            int rb = GetOutpointRounds(COutPoint(b.tx->GetHash(), b.i));
            if (ra != rb)
                return ra > rb;
            return a.tx->vout[a.i].nValue > b.tx->vout[b.i].nValue;
        });
    }

    std::vector<COutput> selectedCoins;
    int selectedInputRounds = 0;

    // Select enough coins to cover the requested mix amount plus fee,
    // but cap the number of inputs to stay within mempool size limits.
    int maxInputs = COINJOIN_MAX_INPUTS;
    for (const auto& out : vCoins) {
        COutPoint outpoint(out.tx->GetHash(), out.i);
        int inputRounds = GetOutpointRounds(outpoint);

        selectedCoins.push_back(out);
        totalValue += out.tx->vout[out.i].nValue;
        selectedInputRounds = std::max(selectedInputRounds, inputRounds);

        if ((int)selectedCoins.size() >= maxInputs) {
            LogPrintf("CoinJoin: Input limit reached (%d inputs), total: %s\n", maxInputs, FormatMoney(totalValue));
            break;
        }

        if (totalValue >= targetInput + fee)
            break;
    }

    if (selectedCoins.empty()) {
        LogPrintf("CoinJoin: No suitable coins found\n");
        return false;
    }

    if (totalValue < 2 * COINJOIN_MIN_MIX_OUTPUT) {
        LogPrintf("CoinJoin: Selected %d inputs totaling %s, too small\n",
                  (int)selectedCoins.size(), FormatMoney(totalValue));
        return false;
    }

    // Privacy-focused mixing: split into outputs with DIFFERENT amounts.
    // Here denomination means maximum mixed output amount, not exact amount.
    // This breaks the "same amount = same owner" heuristic used by chain analysis
    CAmount maxOutputAmount = denomination;

    // Recalculate fee based on actual selected inputs.
    // This prevents "insufficient priority" rejections from mempool.
    // NOTE: this fork's default minRelayTxFee is 10000 sat/kB (10x Bitcoin's
    // 1000), so the estimate must be generous: a signed P2PKH input is
    // ~180 bytes (we budget 200) and we add a 10% safety margin.
    int nInputs = (int)selectedCoins.size();
    int estimatedOutputs = std::max(2, (int)(((mixAmount > 0 ? mixAmount : totalValue) + maxOutputAmount - 1) / maxOutputAmount)) + 2;
    unsigned int estimatedSize = (unsigned int)((nInputs * 200 + estimatedOutputs * 34 + 40) * 1.1);
    fee = std::max(COINJOIN_DEFAULT_FEE, ::minRelayTxFee.GetFee(estimatedSize));

    // If selected inputs don't even cover the estimated fee, fail early.
    if (totalValue < fee + 2 * COINJOIN_MIN_MIX_OUTPUT) {
        LogPrintf("CoinJoin: Selected %d inputs totaling %s, not enough for estimated fee %s\n",
                  nInputs, FormatMoney(totalValue), FormatMoney(fee));
        return false;
    }

    CAmount availableAfterFee = totalValue - fee;
    CAmount amountToMix;
    if (mixAmount > 0) {
        if (availableAfterFee < mixAmount) {
            LogPrintf("CoinJoin: Selected %s after fee, requested %s — mixing what's available\n",
                      FormatMoney(availableAfterFee), FormatMoney(mixAmount));
            amountToMix = availableAfterFee;
        } else {
            amountToMix = mixAmount;
        }
    } else {
        amountToMix = availableAfterFee;
    }
    if (amountToMix <= 0) {
        LogPrintf("CoinJoin: Not enough to cover fee\n");
        return false;
    }

    // Round the mix amount down to the output granularity so the split
    // algorithm can always distribute it exactly; the remainder joins the
    // change output.
    CAmount mixRemainder = amountToMix % COINJOIN_MIN_MIX_OUTPUT;
    CAmount amountToMixRounded = amountToMix - mixRemainder;
    if (amountToMixRounded < 2 * COINJOIN_MIN_MIX_OUTPUT) {
        LogPrintf("CoinJoin: Mixed amount %s too small after rounding\n", FormatMoney(amountToMix));
        return false;
    }

    std::vector<CAmount> outputAmounts = SplitAmountsForMixing(amountToMixRounded, maxOutputAmount);
    if (outputAmounts.empty() || outputAmounts.size() > 1000) {
        LogPrintf("CoinJoin: Failed to split %s into mixed outputs (max %s)\n",
                  FormatMoney(amountToMixRounded), FormatMoney(maxOutputAmount));
        return false;
    }
    int nOutputs = (int)outputAmounts.size();

    // Leftover change: whatever was selected but not mixed (incl. remainder).
    CAmount mixedTotal = 0;
    for (CAmount a : outputAmounts) mixedTotal += a;
    CAmount change = totalValue - fee - mixedTotal;

    // Create transaction
    CMutableTransaction tx;
    tx.nVersion = 1;
    tx.nTime = GetAdjustedTime();

    // Add inputs
    for (const auto& out : selectedCoins) {
        tx.vin.push_back(CTxIn(out.tx->GetHash(), out.i));
    }

    // Generate keys and add outputs with DIFFERENT amounts
    for (int i = 0; i < nOutputs; i++) {
        CPubKey newKey;
        if (!wallet->GetKeyFromPool(newKey)) {
            LogPrintf("CoinJoin: Failed to get key for output %d\n", i);
            return false;
        }
        CScript scriptPubKey = GetScriptForDestination(newKey.GetID());
        tx.vout.push_back(CTxOut(outputAmounts[i], scriptPubKey));
    }

    // Add normal change only when large enough to be worth a output.
    if (change > MIN_CHANGE) {
        CPubKey changeKey;
        if (!wallet->GetKeyFromPool(changeKey)) {
            LogPrintf("CoinJoin: Failed to get change key\n");
            return false;
        }
        CScript changeScript = GetScriptForDestination(changeKey.GetID());
        tx.vout.push_back(CTxOut(change, changeScript));
    } else {
        change = 0; // folds into the fee
    }

    // Snapshot the input scripts/values under the wallet lock, then sign
    // without holding it (mapWallet entries must not be read concurrently
    // with mutations from other threads).
    std::vector<std::pair<CScript, CAmount> > prevouts;
    prevouts.reserve(tx.vin.size());
    {
        LOCK(wallet->cs_wallet);
        for (unsigned int i = 0; i < tx.vin.size(); i++) {
            const COutPoint& outpoint = tx.vin[i].prevout;
            auto wit = wallet->mapWallet.find(outpoint.hash);
            if (wit == wallet->mapWallet.end() || outpoint.n >= wit->second.vout.size()) {
                prevouts.push_back(std::make_pair(CScript(), CAmount(0)));
            } else {
                prevouts.push_back(std::make_pair(wit->second.vout[outpoint.n].scriptPubKey,
                                                  wit->second.vout[outpoint.n].nValue));
            }
        }
    }

    // Sign all inputs
    for (unsigned int i = 0; i < tx.vin.size(); i++) {
        if (prevouts[i].first.empty()) {
            LogPrintf("CoinJoin: Input %d parent tx not found\n", i);
            return false;
        }

        SignatureData sigdata;
        bool signed_ok = ProduceSignature(
            MutableTransactionSignatureCreator(wallet, &tx, i, prevouts[i].second, SIGHASH_ALL),
            prevouts[i].first, sigdata);

        if (!signed_ok) {
            LogPrintf("CoinJoin: Failed to sign input %d\n", i);
            return false;
        }
        UpdateTransaction(tx, i, sigdata);
    }

    // Commit to wallet
    wtxNew = CWalletTx(wallet, CTransaction(tx));
    wtxNew.mapValue["coinjoin"] = "1";

    nFeeRet = fee + (totalValue - fee - mixedTotal - change); // actual fee incl. folded change

    // Mark only randomized mixed outputs, not change.
    int outputRounds = selectedInputRounds + 1;
    uint256 txHash = wtxNew.GetHash();
    {
        LOCK(cs_coinjoin);
        for (unsigned int i = 0; i < outputAmounts.size(); i++) {
            MarkMixedUnlocked(COutPoint(txHash, i), outputRounds);
        }
    }

    // Log output amounts for debugging
    std::string amounts;
    for (size_t i = 0; i < outputAmounts.size(); i++) {
        if (i > 0) amounts += ", ";
        amounts += FormatMoney(outputAmounts[i]);
    }
    LogPrintf("CoinJoin: Created tx %s round %d with %d varied outputs max %s [%s] (input: %s, mixed: %s, change: %s, fee: %s)\n",
              txHash.ToString().substr(0, 16), outputRounds, nOutputs, FormatMoney(maxOutputAmount), amounts.c_str(),
              FormatMoney(totalValue), FormatMoney(mixedTotal), FormatMoney(change), FormatMoney(nFeeRet));

    return true;
}

int CCoinJoin::AutoMix(CWallet* wallet, CAmount denomination, int rounds, CAmount mixAmount)
{
    int nCreated = 0;
    {
        LOCK(cs_coinjoin);
        if (fMixing.load()) {
            LogPrintf("CoinJoin: AutoMix already in progress\n");
            return 0;
        }
        fMixing = true;
        fStopRequested = false;
        config.nDenom = denomination;
        config.nRounds = rounds;
        nLastRoundsRequested = rounds;
        nLastRoundsCompleted = 0;
        nLastTransactionsRequested = 0;
        nLastTransactionsCompleted = 0;
        strLastStatus = strprintf("Mixing started: 0/%d rounds", rounds);
    }

    // RAII guard that clears the mixing flag on every exit path.
    class MixFinishGuard {
        CCoinJoin* m_self;
    public:
        explicit MixFinishGuard(CCoinJoin* s) : m_self(s) {}
        ~MixFinishGuard() { m_self->fMixing = false; }
    } finishGuard(this);

    if (rounds < 1) {
        SetStatus("Mixing failed: rounds must be at least 1");
        LogPrintf("CoinJoin: AutoMix failed - invalid rounds %d\n", rounds);
        return 0;
    }

    CAmount maxTransactionAmount = denomination;
    CAmount totalMixAmount = mixAmount > 0 ? mixAmount : maxTransactionAmount;
    if (maxTransactionAmount <= COINJOIN_DEFAULT_FEE + 2 * COINJOIN_MIN_MIX_OUTPUT) {
        SetStatus("Mixing failed: max transaction amount too small");
        LogPrintf("CoinJoin: AutoMix failed - max transaction amount %s too small\n", FormatMoney(maxTransactionAmount));
        return 0;
    }

    std::vector<CAmount> chunks;
    CAmount remainingToPlan = totalMixAmount;
    while (remainingToPlan > 0) {
        CAmount chunk = std::min(maxTransactionAmount, remainingToPlan);
        if (chunk <= COINJOIN_DEFAULT_FEE + 2 * COINJOIN_MIN_MIX_OUTPUT) {
            // Merge a too-small tail into the previous chunk instead of failing.
            if (!chunks.empty()) {
                chunks.back() += chunk;
                remainingToPlan -= chunk;
                continue;
            }
            SetStatus(strprintf("Mixing failed: remaining amount %s is too small", FormatMoney(chunk)));
            LogPrintf("CoinJoin: AutoMix failed - chunk %s too small\n", FormatMoney(chunk));
            return 0;
        }
        chunks.push_back(chunk);
        remainingToPlan -= chunk;
    }

    {
        LOCK(cs_coinjoin);
        nLastTransactionsRequested = rounds * (int)chunks.size();
    }

    LogPrintf("CoinJoin: AutoMix starting - maxTx: %s, rounds: %d, totalMixAmount: %s, chunks: %d, txTarget: %d\n",
              FormatMoney(denomination), rounds, FormatMoney(totalMixAmount), (int)chunks.size(), rounds * (int)chunks.size());

    // Clean out stale sessions while we're at it.
    CleanupExpiredSessions();

    // Track unconfirmed chain length to avoid mempool chain limit errors.
    // Each broadcast adds to the chain; confirmations reset it.
    int unconfirmedChainLength = 0;

    for (int round = 0; round < rounds; round++) {
        bool roundComplete = true;
        // NOTE: signed index - the chain-limit retry path may rewind it.
        for (int chunkIndex = 0; chunkIndex < (int)chunks.size(); chunkIndex++) {
            if (fStopRequested) {
                SetStatus(strprintf("Mixing stopped by user: %d/%d transactions", nCreated, nLastTransactionsRequested));
                LogPrintf("CoinJoin: AutoMix stopped by user at round %d chunk %d\n", round + 1, chunkIndex + 1);
                roundComplete = false;
                break;
            }

            // Before broadcasting, check if we're approaching the mempool chain limit.
            // If so, wait for confirmations to break the chain.
            if (unconfirmedChainLength >= COINJOIN_MAX_UNCONFIRMED_CHAIN) {
                int startHeight = chainActive.Height();
                SetStatus(strprintf("Waiting for confirmations (%d/%d tx unconfirmed)...", unconfirmedChainLength, COINJOIN_MAX_UNCONFIRMED_CHAIN));
                LogPrintf("CoinJoin: AutoMix pausing - %d unconfirmed txs in chain, waiting for block confirmation\n", unconfirmedChainLength);

                // Poll for new block (max 30 minutes wait)
                bool confirmed = false;
                for (int waitSec = 0; waitSec < 1800 && !fStopRequested; waitSec += 10) {
                    MilliSleep(10000);
                    if (chainActive.Height() > startHeight) {
                        confirmed = true;
                        int newBlocks = chainActive.Height() - startHeight;
                        LogPrintf("CoinJoin: AutoMix resumed after %d block(s) confirmed (height %d → %d)\n",
                                  newBlocks, startHeight, chainActive.Height());
                        break;
                    }
                    // Update status periodically
                    SetStatus(strprintf("Waiting for block confirmation... (%ds)", waitSec));
                }

                if (fStopRequested) {
                    SetStatus(strprintf("Mixing stopped by user while waiting: %d/%d transactions", nCreated, nLastTransactionsRequested));
                    roundComplete = false;
                    break;
                }

                if (!confirmed) {
                    SetStatus(strprintf("Mixing paused: no block after 30 min, %d/%d tx created", nCreated, nLastTransactionsRequested));
                    LogPrintf("CoinJoin: AutoMix timed out waiting for confirmation after 30 minutes\n");
                    roundComplete = false;
                    break;
                }

                // Reset chain counter after confirmation
                unconfirmedChainLength = 0;
            }

            CWalletTx wtxNew;
            CAmount nFee;
            CAmount chunkAmount = chunks[chunkIndex];

            // Later rounds remix previous outputs, which may still be unconfirmed.
            bool allowUnconfirmed = (round > 0);

            SetStatus(strprintf("Mixing round %d/%d, transaction %d/%d", round + 1, rounds,
                                      nCreated + 1, nLastTransactionsRequested));

            if (CreateCoinJoinTransaction(wallet, denomination, wtxNew, nFee, allowUnconfirmed, chunkAmount)) {
                CValidationState state;
                bool broadcastOk = false;
                bool chainTooLong = false;
                {
                    LOCK(cs_main);
                    if (AcceptToMemoryPool(mempool, state, (CTransaction)wtxNew, false, NULL)) {
                        RelayTransaction((CTransaction)wtxNew);
                        broadcastOk = true;
                        LogPrintf("CoinJoin: AutoMix round %d chunk %d - transaction %s broadcast\n",
                                  round + 1, chunkIndex + 1, wtxNew.GetHash().ToString().substr(0, 16));
                    } else {
                        std::string reason = state.GetRejectReason();
                        if (reason == "too-long-mempool-chain") {
                            chainTooLong = true;
                        }
                        CTransaction txr((CTransaction)wtxNew);
                        LogPrintf("CoinJoin: AutoMix round %d chunk %d - failed to broadcast: %s (size=%d fee=%d in=%d out=%d vin=%d)\n",
                                  round + 1, chunkIndex + 1, reason,
                                  (int)::GetSerializeSize(txr, SER_NETWORK, PROTOCOL_VERSION), (int)nFee,
                                  (int)txr.GetValueOut() + (int)nFee, (int)txr.GetValueOut(), (int)txr.vin.size());
                    }
                }

                if (broadcastOk) {
                    wallet->AddToWallet(wtxNew, true, NULL);
                    nCreated++;
                    {
                        LOCK(cs_coinjoin);
                        nLastTransactionsCompleted = nCreated;
                        nTotalMixed += chunkAmount;
                        nTotalSessions++;
                        nTotalFees += nFee;
                    }
                    unconfirmedChainLength++;
                    SetStatus(strprintf("Mixing transaction %d/%d broadcast (%d unconfirmed)", nCreated, nLastTransactionsRequested, unconfirmedChainLength));
                } else if (chainTooLong) {
                    // Mempool chain limit hit — wait for confirmations and retry this chunk
                    int startHeight = chainActive.Height();
                    SetStatus("Mempool chain full, waiting for confirmation...");
                    LogPrintf("CoinJoin: AutoMix hit mempool chain limit, waiting for block confirmation\n");

                    bool confirmed = false;
                    for (int waitSec = 0; waitSec < 1800 && !fStopRequested; waitSec += 10) {
                        MilliSleep(10000);
                        if (chainActive.Height() > startHeight) {
                            confirmed = true;
                            LogPrintf("CoinJoin: AutoMix resumed after block confirmed (height %d → %d)\n",
                                      startHeight, chainActive.Height());
                            break;
                        }
                        SetStatus(strprintf("Waiting for block to clear mempool chain... (%ds)", waitSec));
                    }

                    if (fStopRequested) {
                        SetStatus(strprintf("Mixing stopped while waiting: %d/%d transactions", nCreated, nLastTransactionsRequested));
                        roundComplete = false;
                        break;
                    }

                    if (confirmed) {
                        unconfirmedChainLength = 0;
                        // Retry this same chunk (don't advance chunkIndex)
                        chunkIndex--;
                        continue;
                    } else {
                        SetStatus(strprintf("Mixing timed out waiting for confirmation: %d/%d tx", nCreated, nLastTransactionsRequested));
                        roundComplete = false;
                        break;
                    }
                } else {
                    SetStatus(strprintf("Mixing failed at transaction %d/%d: %s", nCreated + 1,
                                              nLastTransactionsRequested, state.GetRejectReason()));
                    LogPrintf("CoinJoin: AutoMix transaction %d - broadcast failed, stopping (tx NOT saved to wallet)\n", nCreated + 1);
                    roundComplete = false;
                    break;
                }
            } else {
                SetStatus(strprintf("Mixing stopped at transaction %d/%d: no suitable coins", nCreated + 1,
                                          nLastTransactionsRequested));
                LogPrintf("CoinJoin: AutoMix transaction %d - no suitable coins, stopping\n", nCreated + 1);
                roundComplete = false;
                break;
            }
        }

        if (roundComplete) {
            LOCK(cs_coinjoin);
            nLastRoundsCompleted = round + 1;
        } else {
            break;
        }
    }

    std::string finalStatus;
    {
        LOCK(cs_coinjoin);
        if (nCreated == nLastTransactionsRequested)
            strLastStatus = strprintf("Mixing complete: %d/%d rounds, %d/%d transactions", nLastRoundsCompleted,
                                      nLastRoundsRequested, nCreated, nLastTransactionsRequested);
        else if (nCreated > 0 &&
                 strLastStatus.find("failed") == std::string::npos &&
                 strLastStatus.find("stopped") == std::string::npos &&
                 strLastStatus.find("timed out") == std::string::npos)
            strLastStatus = strprintf("Mixing partial: %d/%d transactions", nCreated, nLastTransactionsRequested);
        finalStatus = strLastStatus;
    }
    LogPrintf("CoinJoin: AutoMix completed - %d transactions created (%s)\n", nCreated, finalStatus);
    return nCreated;
}

void CCoinJoin::StopMixing()
{
    {
        LOCK(cs_coinjoin);
        fStopRequested = true;
        strLastStatus = "Mixing stop requested";
    }
    LogPrintf("CoinJoin: Stop mixing requested\n");
}

double CCoinJoin::GetMixingProgress(CWallet* wallet, CAmount denomination)
{
    if (denomination == 0)
        return 0.0;

    int nRoundsTarget = COINJOIN_DEFAULT_ROUNDS;
    bool active = false;
    {
        LOCK(cs_coinjoin);
        nRoundsTarget = std::max(1, config.nRounds);
        if (nLastTransactionsRequested > 0 && denomination == config.nDenom) {
            return std::min(100.0, (double)nLastTransactionsCompleted / (double)nLastTransactionsRequested * 100.0);
        }
        active = fMixing.load();
    }
    (void)active;

    std::vector<COutput> vCoins;
    wallet->AvailableCoins(vCoins, true, NULL, false);

    int nTotal = 0;
    int nMixed = 0;

    for (const auto& out : vCoins) {
        CAmount val = out.tx->vout[out.i].nValue;
        // Coins big enough to count toward this denomination's mixing pool
        if (val >= denomination) {
            nTotal++;
            COutPoint outpoint(out.tx->GetHash(), out.i);
            if (GetOutpointRounds(outpoint) >= nRoundsTarget)
                nMixed++;
        }
    }

    if (nTotal == 0)
        return 0.0;

    return (double)nMixed / (double)nTotal * 100.0;
}

CAmount CCoinJoin::SuggestDenomination(CWallet* wallet, bool allowUnconfirmed) const
{
    std::vector<COutput> vCoins;
    wallet->AvailableCoins(vCoins, !allowUnconfirmed, NULL, false);

    if (vCoins.empty())
        return 0;

    // Find the largest coin value
    CAmount maxCoinValue = 0;
    CAmount totalValue = 0;
    for (const auto& out : vCoins) {
        totalValue += out.tx->vout[out.i].nValue;
        if (out.tx->vout[out.i].nValue > maxCoinValue) {
            maxCoinValue = out.tx->vout[out.i].nValue;
        }
    }

    // Suggest the largest denomination that is <= the largest coin
    std::vector<CAmount> denoms = GetStandardDenominations();
    for (int i = (int)denoms.size() - 1; i >= 0; i--) {
        if (denoms[i] <= maxCoinValue) {
            return denoms[i];
        }
    }

    // If no single coin is large enough, suggest smallest denomination
    // that total balance can afford (need at least 2x for mixing to make sense)
    for (size_t i = 0; i < denoms.size(); i++) {
        if (totalValue >= denoms[i] * 2) {
            return denoms[i];
        }
    }

    return 0;
}

std::pair<CAmount, int> CCoinJoin::GetAvailableBalance(CWallet* wallet, bool allowUnconfirmed) const
{
    std::vector<COutput> vCoins;
    wallet->AvailableCoins(vCoins, !allowUnconfirmed, NULL, false);

    CAmount total = 0;
    int count = 0;
    for (const auto& out : vCoins) {
        total += out.tx->vout[out.i].nValue;
        count++;
    }

    return std::make_pair(total, count);
}

int CCoinJoin::GetOutpointRoundsUnlocked(const COutPoint& outpoint) const
{
    auto it = mapOutpointRounds.find(outpoint);
    if (it != mapOutpointRounds.end())
        return it->second;
    return 0;
}

void CCoinJoin::MarkMixedUnlocked(const COutPoint& outpoint, int rounds)
{
    setMixedOutpoints.insert(outpoint);
    mapOutpointRounds[outpoint] = rounds;
}

CoinJoinSession CCoinJoin::GetSession(const uint256& sessionId) const
{
    LOCK(cs_coinjoin);
    auto it = mapSessions.find(sessionId);
    if (it != mapSessions.end())
        return it->second;
    return CoinJoinSession();
}

std::vector<CoinJoinSession> CCoinJoin::GetActiveSessions() const
{
    LOCK(cs_coinjoin);
    std::vector<CoinJoinSession> active;
    for (const auto& pair : mapSessions) {
        if (pair.second.state != CJ_STATE_COMPLETE &&
            pair.second.state != CJ_STATE_ERROR &&
            !pair.second.IsExpired()) {
            active.push_back(pair.second);
        }
    }
    return active;
}

bool CCoinJoin::CancelSession(const uint256& sessionId)
{
    LOCK(cs_coinjoin);
    return mapSessions.erase(sessionId) > 0;
}

void CCoinJoin::CleanupExpiredSessions()
{
    LOCK(cs_coinjoin);
    std::vector<uint256> toRemove;

    for (const auto& pair : mapSessions) {
        if (pair.second.IsExpired() || pair.second.state == CJ_STATE_COMPLETE) {
            toRemove.push_back(pair.first);
        }
    }

    for (const auto& id : toRemove) {
        mapSessions.erase(id);
    }

    if (!toRemove.empty()) {
        LogPrintf("CoinJoin: Cleaned up %d expired sessions\n", toRemove.size());
    }
}

bool CCoinJoin::CreateCollateral(CWallet* wallet)
{
    LOCK2(cs_main, wallet->cs_wallet);

    // Collateral is a small fixed amount to prevent DoS
    CAmount collateralAmount = COINJOIN_DENOM_1; // 0.001 KOT

    // Find suitable UTXO
    std::vector<COutput> vCoins;
    wallet->AvailableCoins(vCoins, true, NULL, false);

    for (const auto& out : vCoins) {
        if (out.tx->vout[out.i].nValue == collateralAmount) {
            CoinJoinCollateral collateral;
            collateral.outpoint = COutPoint(out.tx->GetHash(), out.i);
            collateral.amount = collateralAmount;
            LOCK(cs_coinjoin);
            vecCollateral.push_back(collateral);
            return true;
        }
    }

    // Need to create collateral output
    // This would be done in a separate transaction
    LogPrintf("CoinJoin: No suitable collateral found, need to create one\n");
    return false;
}

bool CCoinJoin::HasCollateral() const
{
    LOCK(cs_coinjoin);
    return !vecCollateral.empty();
}

bool CoinJoinCollateral::IsExpired() const
{
    // Collateral expires after 24 hours
    return false; // Simplified - would check block height/time
}

// Global instance initialization
static CCoinJoin g_coinjoin;

// Helper functions
std::string FormatDenomination(CAmount amount)
{
    if (amount == COINJOIN_DENOM_1) return "0.001";
    if (amount == COINJOIN_DENOM_2) return "0.01";
    if (amount == COINJOIN_DENOM_3) return "0.1";
    if (amount == COINJOIN_DENOM_4) return "1";
    if (amount == COINJOIN_DENOM_5) return "10";
    return FormatMoney(amount);
}

std::vector<CAmount> GetStandardDenominations()
{
    std::vector<CAmount> denoms;
    denoms.push_back(COINJOIN_DENOM_1);
    denoms.push_back(COINJOIN_DENOM_2);
    denoms.push_back(COINJOIN_DENOM_3);
    denoms.push_back(COINJOIN_DENOM_4);
    denoms.push_back(COINJOIN_DENOM_5);
    return denoms;
}
