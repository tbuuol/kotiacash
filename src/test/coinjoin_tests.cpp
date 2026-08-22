// Copyright (c) 2024 Kotia developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "coinjoin.h"

#include "amount.h"
#include "chainparams.h"
#include "main.h"
#include "primitives/transaction.h"
#include "pubkey.h"
#include "random.h"
#include "serialize.h"
#include "streams.h"
#include "test/test_bitcoin.h"
#include "txmempool.h"
#include "utilmoneystr.h"
#include "wallet/wallet.h"

#include <boost/test/unit_test.hpp>

#include <map>
#include <vector>

BOOST_FIXTURE_TEST_SUITE(coinjoin_tests, TestChain100Setup)

// ---------------------------------------------------------------------------
// Pure logic (no chain, no wallet)
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(coinjoin_denominations)
{
    std::vector<CAmount> denoms = GetStandardDenominations();
    BOOST_CHECK_EQUAL(denoms.size(), 5U);
    BOOST_CHECK_EQUAL(denoms[0], COINJOIN_DENOM_1);
    BOOST_CHECK_EQUAL(denoms[4], COINJOIN_DENOM_5);

    CCoinJoin cj;
    BOOST_CHECK(cj.IsValidDenomination(COINJOIN_DENOM_1));
    BOOST_CHECK(cj.IsValidDenomination(COINJOIN_DENOM_3));
    BOOST_CHECK(cj.IsValidDenomination(COINJOIN_DENOM_5));
    BOOST_CHECK(!cj.IsValidDenomination(0));
    BOOST_CHECK(!cj.IsValidDenomination(12345));
    BOOST_CHECK(!cj.IsValidDenomination(COINJOIN_DENOM_5 + 1));

    BOOST_CHECK_EQUAL(FormatDenomination(COINJOIN_DENOM_1), std::string("0.001"));
    BOOST_CHECK_EQUAL(FormatDenomination(COINJOIN_DENOM_2), std::string("0.01"));
    BOOST_CHECK_EQUAL(FormatDenomination(COINJOIN_DENOM_3), std::string("0.1"));
    BOOST_CHECK_EQUAL(FormatDenomination(COINJOIN_DENOM_4), std::string("1"));
    BOOST_CHECK_EQUAL(FormatDenomination(COINJOIN_DENOM_5), std::string("10"));
    // Unknown denominations fall back to money formatting
    BOOST_CHECK_EQUAL(FormatDenomination(123456), FormatMoney(123456));

    BOOST_CHECK(cj.GetDenominations() == denoms);
}

BOOST_AUTO_TEST_CASE(coinjoin_session_lifecycle)
{
    CCoinJoin cj;

    // Invalid denomination rejected
    uint256 sessionId;
    BOOST_CHECK(!cj.CreateSession(0, sessionId));
    BOOST_CHECK(!cj.CreateSession(1234, sessionId));

    // Valid session
    BOOST_CHECK(cj.CreateSession(COINJOIN_DENOM_2, sessionId));
    BOOST_CHECK(!sessionId.IsNull());

    CoinJoinSession session = cj.GetSession(sessionId);
    BOOST_CHECK(session.sessionId == sessionId);
    BOOST_CHECK_EQUAL(session.denomination, COINJOIN_DENOM_2);
    BOOST_CHECK(session.state == CJ_STATE_WAITING_FOR_PARTICIPANTS);
    BOOST_CHECK_EQUAL(session.GetParticipantCount(), 0);
    BOOST_CHECK(!session.IsExpired()); // fresh session

    // Joining participants
    std::vector<COutPoint> inputs;
    std::vector<CTxOut> outputs;
    outputs.push_back(CTxOut(COINJOIN_DENOM_2, CScript()));
    BOOST_CHECK(cj.JoinSession(sessionId, inputs, outputs));
    BOOST_CHECK(cj.JoinSession(sessionId, inputs, outputs));

    session = cj.GetSession(sessionId);
    BOOST_CHECK_EQUAL(session.GetParticipantCount(), 2);
    // Reached min participants -> inputs registered
    BOOST_CHECK(session.state == CJ_STATE_INPUTS_REGISTERED);

    // Fill to the cap
    for (int i = 0; i < COINJOIN_MAX_PARTICIPANTS - 2; ++i)
        BOOST_CHECK(cj.JoinSession(sessionId, inputs, outputs));
    // One over the cap fails
    BOOST_CHECK(!cj.JoinSession(sessionId, inputs, outputs));

    session = cj.GetSession(sessionId);
    BOOST_CHECK_EQUAL(session.GetParticipantCount(), COINJOIN_MAX_PARTICIPANTS);

    // Output value mismatch rejected
    std::vector<CTxOut> wrongOutputs;
    wrongOutputs.push_back(CTxOut(COINJOIN_DENOM_3, CScript()));
    BOOST_CHECK(!cj.SubmitOutputs(sessionId, uint256(), wrongOutputs));

    // Unknown sessions
    BOOST_CHECK(!cj.JoinSession(uint256S("0x1"), inputs, outputs));
    BOOST_CHECK(!cj.SubmitOutputs(uint256S("0x1"), uint256(), outputs));
    BOOST_CHECK(!cj.GetActiveSessions().empty());
    BOOST_CHECK(cj.CancelSession(sessionId));
    BOOST_CHECK(!cj.CancelSession(sessionId));

    // Not yet signed/submitted states
    CoinJoinSession s2;
    s2.AddParticipant(uint256S("0xa"), CoinJoinParticipant());
    s2.AddParticipant(uint256S("0xb"), CoinJoinParticipant());
    BOOST_CHECK(!s2.AllInputsRegistered());
    BOOST_CHECK(!s2.AllOutputsSubmitted());
}

BOOST_AUTO_TEST_CASE(coinjoin_session_expiry_and_cleanup)
{
    CCoinJoin cj;
    uint256 sessionId;
    BOOST_CHECK(cj.CreateSession(COINJOIN_DENOM_1, sessionId));

    // Session with activity 10 minutes ago is expired (timeout is 300s)
    {
        // Reach into the session copy to test the expiry logic directly.
        CoinJoinSession session = cj.GetSession(sessionId);
        session.nTimeLastActivity = GetTime() - 600;
        BOOST_CHECK(session.IsExpired());

        session.nTimeLastActivity = GetTime();
        BOOST_CHECK(!session.IsExpired());

        // Zero-activity sessions are never considered expired (guards the
        // constructor default of 0 against instant expiry).
        session.nTimeLastActivity = 0;
        BOOST_CHECK(!session.IsExpired());
    }

    // CleanupExpiredSessions drops completed sessions
    uint256 doneSession;
    cj.CreateSession(COINJOIN_DENOM_2, doneSession);
    std::vector<CoinJoinSession> active = cj.GetActiveSessions();
    BOOST_CHECK_EQUAL(active.size(), 2U);
    cj.CleanupExpiredSessions();
    active = cj.GetActiveSessions();
    BOOST_CHECK_EQUAL(active.size(), 2U); // none expired yet
}

BOOST_AUTO_TEST_CASE(coinjoin_session_serialization)
{
    CoinJoinSession session;
    session.sessionId = uint256S("0xdeadbeef");
    session.denomination = COINJOIN_DENOM_4;
    session.nRounds = 3;
    session.nMinParticipants = 2;
    session.nMaxParticipants = 7;
    session.nTimeCreated = 1000;
    session.nTimeLastActivity = 2000;
    session.state = CJ_STATE_OUTPUTS_SUBMITTED;

    CoinJoinParticipant p;
    p.pubKey = CPubKey();
    p.inputs.push_back(COutPoint(uint256S("0x1"), 0));
    p.outputs.push_back(CTxOut(COIN, CScript() << OP_TRUE));
    p.inputsSigned = true;
    p.outputsSubmitted = false;
    session.participants[uint256S("0xabc")] = p;

    CDataStream ss(SER_DISK, CLIENT_VERSION);
    ss << session;

    CoinJoinSession session2;
    ss >> session2;

    BOOST_CHECK(session2.sessionId == session.sessionId);
    BOOST_CHECK_EQUAL(session2.denomination, session.denomination);
    BOOST_CHECK_EQUAL(session2.nRounds, session.nRounds);
    BOOST_CHECK_EQUAL(session2.nMinParticipants, session.nMinParticipants);
    BOOST_CHECK_EQUAL(session2.nMaxParticipants, session.nMaxParticipants);
    BOOST_CHECK_EQUAL(session2.nTimeCreated, session.nTimeCreated);
    BOOST_CHECK_EQUAL(session2.nTimeLastActivity, session.nTimeLastActivity);
    BOOST_CHECK(session2.state == CJ_STATE_OUTPUTS_SUBMITTED);
    BOOST_CHECK_EQUAL(session2.participants.size(), 1U);
    BOOST_CHECK(session2.participants.count(uint256S("0xabc")));
    BOOST_CHECK_EQUAL(session2.participants[uint256S("0xabc")].inputs.size(), 1U);
    BOOST_CHECK_EQUAL(session2.participants[uint256S("0xabc")].outputs.size(), 1U);
    BOOST_CHECK_EQUAL(session2.participants[uint256S("0xabc")].outputs[0].nValue, COIN);
    BOOST_CHECK(session2.participants[uint256S("0xabc")].inputsSigned);
    BOOST_CHECK(!session2.participants[uint256S("0xabc")].outputsSubmitted);
}

BOOST_AUTO_TEST_CASE(coinjoin_split_amounts_invariants)
{
    // Helper checking all documented invariants of the splitter.
    struct Checker {
        static void check(CAmount amountToMix, CAmount maxOutput) {
            std::vector<CAmount> amounts = CCoinJoin::SplitAmountsForMixing(amountToMix, maxOutput);
            BOOST_REQUIRE(!amounts.empty());
            CAmount sum = 0;
            for (CAmount a : amounts) {
                // every output respects the bounds
                BOOST_CHECK_GE(a, COINJOIN_MIN_MIX_OUTPUT);
                BOOST_CHECK_LE(a, maxOutput);
                sum += a;
            }
            // exact total
            BOOST_CHECK_EQUAL(sum, amountToMix);
            // at least two outputs (amountToMix >= 2*MIN is required upstream)
            BOOST_CHECK_GE(amounts.size(), 2U);
        }
    };

    // Explicit edge cases
    BOOST_CHECK(CCoinJoin::SplitAmountsForMixing(0, COINJOIN_DENOM_2).empty());
    BOOST_CHECK(CCoinJoin::SplitAmountsForMixing(-5, COINJOIN_DENOM_2).empty());
    BOOST_CHECK(CCoinJoin::SplitAmountsForMixing(COINJOIN_MIN_MIX_OUTPUT, COINJOIN_DENOM_2).empty());      // < 2*MIN
    BOOST_CHECK(CCoinJoin::SplitAmountsForMixing(2 * COINJOIN_MIN_MIX_OUTPUT - 1, COINJOIN_DENOM_2).empty()); // not splittable
    BOOST_CHECK(CCoinJoin::SplitAmountsForMixing(2 * COINJOIN_MIN_MIX_OUTPUT, 999).empty());               // max < MIN
    BOOST_CHECK(CCoinJoin::SplitAmountsForMixing(2 * COINJOIN_MIN_MIX_OUTPUT, COINJOIN_MIN_MIX_OUTPUT).size() == 2U);

    Checker::check(2 * COINJOIN_MIN_MIX_OUTPUT, COINJOIN_MIN_MIX_OUTPUT);
    Checker::check(3 * COINJOIN_MIN_MIX_OUTPUT, COINJOIN_MIN_MIX_OUTPUT * 3 / 2); // previously produced 1000+2000 (>max)
    Checker::check(COINJOIN_DENOM_2, COINJOIN_DENOM_2);
    Checker::check(COINJOIN_DENOM_5, COINJOIN_DENOM_4); // 10 KOT with 1 KOT max
    Checker::check(10 * COINJOIN_DENOM_5, COINJOIN_DENOM_5);

    // Stress: random amounts must always satisfy the invariants.
    // (GetRandInt takes an int - keep the range below 2^31.)
    for (int i = 0; i < 500; i++) {
        CAmount maxOut = (CAmount)(GetRandInt(1000000) + COINJOIN_MIN_MIX_OUTPUT);
        CAmount amount = (CAmount)(GetRandInt(2000000000) + 2 * COINJOIN_MIN_MIX_OUTPUT);
        amount = amount - (amount % COINJOIN_MIN_MIX_OUTPUT) + 2 * COINJOIN_MIN_MIX_OUTPUT;
        Checker::check(amount, maxOut);
    }
}

BOOST_AUTO_TEST_CASE(coinjoin_outpoint_rounds_tracking)
{
    CCoinJoin cj;
    COutPoint outpoint(uint256S("0x1234"), 5);

    BOOST_CHECK_EQUAL(cj.GetOutpointRounds(outpoint), 0);
    cj.MarkMixed(outpoint, 1);
    BOOST_CHECK_EQUAL(cj.GetOutpointRounds(outpoint), 1);
    cj.MarkMixed(outpoint, 3);
    BOOST_CHECK_EQUAL(cj.GetOutpointRounds(outpoint), 3);
}

// ---------------------------------------------------------------------------
// Integration with a real wallet on the regtest chain
// ---------------------------------------------------------------------------

namespace {
// Build an in-memory (non file-backed) wallet funded with the matured
// coinbase transactions created by TestChain100Setup.
CWallet* FundedTestWallet(const CKey& key, const std::vector<CTransaction>& txns)
{
    // Default ctor -> in-memory wallet (fFileBacked = false): no BerkeleyDB
    // env needed, keypool works in memory.
    CWallet* wallet = new CWallet();
    wallet->AddKey(key);

    // Scan the chain into the wallet: this adds the coinbase txs WITH their
    // merkle branches (hashBlock), so GetDepthInMainChain() works and the
    // matured coinbases become spendable.
    LOCK(wallet->cs_wallet);
    wallet->ScanForWalletTransactions(chainActive.Genesis(), true);

    return wallet;
}
} // namespace

#define MAKE_TEST_WALLET() FundedTestWallet(coinbaseKey, coinbaseTxns)

// NOTE: all wallet/chain-dependent checks live in ONE test case because the
// TestChain100Setup fixture mines 10 scrypt-proof-of-work blocks (slow), and
// the fixture would otherwise be rebuilt for every single test case.
BOOST_AUTO_TEST_CASE(coinjoin_wallet_integration)
{
    CWallet* wallet = MAKE_TEST_WALLET();

    // ------------------------------------------------------------
    // Balance / denomination suggestions
    // ------------------------------------------------------------
    // Chain pays 100000 KOT (block 1) and 100 KOT (blocks 2..10); with
    // nCoinbaseMaturity == 10 only the block-1 coinbase is spendable.
    std::pair<CAmount, int> bal = wallet->GetCoinJoinAvailableBalance(true);
    BOOST_CHECK_GT(bal.first, 0);
    BOOST_CHECK_GE(bal.second, 1);

    CAmount suggested = wallet->SuggestCoinJoinDenomination(true);
    BOOST_CHECK_GT(suggested, 0);
    BOOST_CHECK(wallet->coinJoin.IsValidDenomination(suggested));

    // ------------------------------------------------------------
    // Single transaction creation
    // ------------------------------------------------------------
    CWalletTx wtx;
    CAmount fee = 0;
    BOOST_CHECK(wallet->CreateCoinJoinTx(suggested, wtx, fee, false, suggested));

    BOOST_CHECK_EQUAL(wtx.mapValue.count("coinjoin"), 1U);
    BOOST_CHECK_GE(fee, COINJOIN_DEFAULT_FEE);

    // Transaction balances: inputs cover outputs + fee
    CAmount inValue = 0, outValue = 0;
    {
        LOCK(wallet->cs_wallet);
        BOOST_FOREACH(const CTxIn& txin, wtx.vin) {
            auto it = wallet->mapWallet.find(txin.prevout.hash);
            BOOST_REQUIRE(it != wallet->mapWallet.end());
            inValue += it->second.vout[txin.prevout.n].nValue;
        }
    }
    BOOST_FOREACH(const CTxOut& out, wtx.vout)
        outValue += out.nValue;
    BOOST_CHECK_EQUAL(inValue - outValue, fee);

    // Mixed outputs: at least two within the denomination cap; the final
    // output may be the change, which is allowed to exceed the cap.
    std::vector<CAmount> mixedAmounts;
    for (unsigned int i = 0; i < wtx.vout.size(); i++)
        mixedAmounts.push_back(wtx.vout[i].nValue);
    int nWithinCap = 0;
    BOOST_FOREACH(CAmount a, mixedAmounts) {
        if (a <= suggested) {
            nWithinCap++;
            BOOST_CHECK_GE(a, COINJOIN_MIN_MIX_OUTPUT);
        }
    }
    BOOST_CHECK_GE(nWithinCap, 2);

    // Invalid parameters must be rejected, not crash
    CWalletTx wtx2;
    BOOST_CHECK(!wallet->CreateCoinJoinTx(0, wtx2, fee, false, 0));            // zero denom
    BOOST_CHECK(!wallet->CreateCoinJoinTx(1000, wtx2, fee, false, 0));         // dust denom
    BOOST_CHECK(!wallet->CreateCoinJoinTx(COINJOIN_DENOM_2, wtx2, fee, false, (CAmount)-1)); // negative amount

    // ------------------------------------------------------------
    // Full AutoMix run
    // ------------------------------------------------------------
    CAmount denom = COINJOIN_DENOM_4; // 1 KOT max tx
    int rounds = 2;
    CAmount mixAmount = denom * 3;    // 3 chunks per round
    int created = wallet->AutoMixCoins(denom, rounds, mixAmount);
    BOOST_CHECK_EQUAL(created, rounds * 3);
    BOOST_CHECK_EQUAL(wallet->coinJoin.GetLastTransactionsRequested(), rounds * 3);
    BOOST_CHECK_EQUAL(wallet->coinJoin.GetLastTransactionsCompleted(), rounds * 3);
    BOOST_CHECK_EQUAL(wallet->coinJoin.GetLastRoundsCompleted(), rounds);
    BOOST_CHECK(!wallet->IsCoinJoinMixing());

    // All created transactions are in the mempool and wallet
    BOOST_CHECK_EQUAL(mempool.size(), (size_t)(rounds * 3));

    // Statistics updated
    BOOST_CHECK_EQUAL(wallet->coinJoin.GetTotalSessions(), (int64_t)(rounds * 3));
    BOOST_CHECK_GE(wallet->coinJoin.GetTotalMixed(), mixAmount);
    BOOST_CHECK_GE(wallet->coinJoin.GetTotalFees(), COINJOIN_DEFAULT_FEE * (int64_t)rounds * 3);

    // Mixed outputs gained rounds through the remixes
    int mixedOutpoints = 0;
    {
        LOCK(cs_main);
        BOOST_FOREACH(const PAIRTYPE(const uint256, CWalletTx)& entry, wallet->mapWallet) {
            if (entry.second.mapValue.count("coinjoin")) {
                for (unsigned int i = 0; i < entry.second.vout.size(); i++) {
                    COutPoint out(entry.first, i);
                    if (wallet->coinJoin.GetOutpointRounds(out) > 0)
                        mixedOutpoints++;
                }
            }
        }
    }
    BOOST_CHECK_GE(mixedOutpoints, 6);

    // Mine blocks until every unconfirmed wallet tx is confirmed
    {
        int nMined = 0;
        for (int guard = 0; guard < 30; guard++) {
            std::vector<CMutableTransaction> txs;
            {
                LOCK2(cs_main, wallet->cs_wallet);
                BOOST_FOREACH(const PAIRTYPE(const uint256, CWalletTx)& entry, wallet->mapWallet) {
                    if (entry.second.GetDepthInMainChain() == 0 && entry.second.InMempool())
                        txs.push_back(entry.second);
                }
            }
            if (txs.empty())
                break;
            CreateAndProcessBlock(txs, GetScriptForDestination(coinbaseKey.GetPubKey().GetID()));
            nMined++;
        }
        BOOST_CHECK_GT(nMined, 0);
    }

    // Progress reported for the mixing denomination
    double progress = wallet->GetCoinJoinProgress(denom);
    BOOST_CHECK_GE(progress, 0.0);
    BOOST_CHECK_LE(progress, 100.0);
    // No division-by-zero crash for degenerate denominations
    BOOST_CHECK_EQUAL(wallet->GetCoinJoinProgress(0), 0.0);

    // ------------------------------------------------------------
    // Invalid AutoMix parameters
    // ------------------------------------------------------------
    // Rounds must be >= 1
    BOOST_CHECK_EQUAL(wallet->AutoMixCoins(denom, 0, 0), 0);
    BOOST_CHECK(!wallet->IsCoinJoinMixing());

    // Denomination too small to split
    BOOST_CHECK_EQUAL(wallet->AutoMixCoins(1000, 2, 0), 0);
    BOOST_CHECK(!wallet->IsCoinJoinMixing());

    // ------------------------------------------------------------
    // Stop handling
    // ------------------------------------------------------------
    // Request stop from a "UI thread" while mixing runs
    boost::thread stopper([&wallet]() {
        MilliSleep(1);
        wallet->StopCoinJoinMixing();
    });

    // Many rounds so the stop has time to land
    int createdMany = wallet->AutoMixCoins(denom, 50, 0);
    stopper.join();

    BOOST_CHECK_LE(createdMany, 50);
    BOOST_CHECK(!wallet->IsCoinJoinMixing());
    std::string status = wallet->GetCoinJoinLastStatus();
    BOOST_CHECK(status.find("stopped") != std::string::npos ||
                status.find("complete") != std::string::npos ||
                status.find("partial") != std::string::npos);

    // ------------------------------------------------------------
    // Thread-safe status reads during mixing
    // ------------------------------------------------------------
    // Mine the leftovers from the stop test so the mempool chain is clean.
    for (int guard = 0; guard < 30; guard++) {
        std::vector<CMutableTransaction> txs;
        {
            LOCK2(cs_main, wallet->cs_wallet);
            BOOST_FOREACH(const PAIRTYPE(const uint256, CWalletTx)& entry, wallet->mapWallet) {
                if (entry.second.GetDepthInMainChain() == 0 && entry.second.InMempool())
                    txs.push_back(entry.second);
            }
        }
        if (txs.empty())
            break;
        CreateAndProcessBlock(txs, GetScriptForDestination(coinbaseKey.GetPubKey().GetID()));
    }

    boost::thread reader([&wallet]() {
        for (int i = 0; i < 200; i++) {
            wallet->GetCoinJoinLastStatus();
            wallet->coinJoin.GetLastTransactionsRequested();
            wallet->coinJoin.GetTotalMixed();
            MilliSleep(2);
        }
    });

    wallet->AutoMixCoins(COINJOIN_DENOM_3, 3, 0);
    reader.join();
    BOOST_CHECK(!wallet->IsCoinJoinMixing());

    delete wallet;
}

BOOST_AUTO_TEST_SUITE_END()
