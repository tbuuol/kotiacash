// Copyright (c) 2011-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#define BOOST_TEST_MODULE Bitcoin Test Suite

#include "test_bitcoin.h"

#include "chainparams.h"
#include "consensus/consensus.h"
#include "consensus/validation.h"
#include "key.h"
#include "main.h"
#include "miner.h"
#include "pubkey.h"
#include "random.h"
#include "txdb.h"
#include "txmempool.h"
#include "ui_interface.h"
#include "util.h"
#include "rpc/server.h"
#include "rpc/register.h"

#include "test/testutil.h"

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>
#include <algorithm>
#include <atomic>
#include <thread>

extern bool fPrintToConsole;
extern void noui_connect();

BasicTestingSetup::BasicTestingSetup(const std::string& chainName)
{
        ECC_Start();
        SetupEnvironment();
        SetupNetworking();
        fPrintToDebugLog = false; // don't want to write to debug.log file
        fPrintToConsole = getenv("KOTIA_TEST_LOG") != NULL;
        fCheckBlockIndex = true;
        SelectParams(chainName);
        noui_connect();
}

BasicTestingSetup::~BasicTestingSetup()
{
        ECC_Stop();
}

TestingSetup::TestingSetup(const std::string& chainName) : BasicTestingSetup(chainName)
{
    const CChainParams& chainparams = Params();
        // Ideally we'd move all the RPC tests to the functional testing framework
        // instead of unit tests, but for now we need these here.
        RegisterAllCoreRPCCommands(tableRPC);
        ClearDatadirCache();
        pathTemp = GetTempPath() / strprintf("test_bitcoin_%lu_%i", (unsigned long)GetTime(), (int)(GetRand(100000)));
        boost::filesystem::create_directories(pathTemp);
        mapArgs["-datadir"] = pathTemp.string();
        mempool.setSanityCheck(1.0);
        pblocktree = new CBlockTreeDB(1 << 20, true);
        pcoinsdbview = new CCoinsViewDB(1 << 23, true);
        pcoinsTip = new CCoinsViewCache(pcoinsdbview);
        InitBlockIndex(chainparams);
        {
            CValidationState state;
            bool ok = ActivateBestChain(state, chainparams);
            BOOST_CHECK(ok);
        }
        nScriptCheckThreads = 3;
        for (int i=0; i < nScriptCheckThreads-1; i++)
            threadGroup.create_thread(&ThreadScriptCheck);
        RegisterNodeSignals(GetNodeSignals());
}

TestingSetup::~TestingSetup()
{
        UnregisterNodeSignals(GetNodeSignals());
        threadGroup.interrupt_all();
        threadGroup.join_all();
        UnloadBlockIndex();
        delete pcoinsTip;
        delete pcoinsdbview;
        delete pblocktree;
        boost::filesystem::remove_all(pathTemp);
}

TestChain100Setup::TestChain100Setup() : TestingSetup(CBaseChainParams::REGTEST)
{
    // Generate a chain with enough blocks for the first coinbase to be
    // spendable: the height-1 coinbase matures once the tip is
    // nCoinbaseMaturity + 1 blocks high (depth counts exclude the tip).
    coinbaseKey.MakeNewKey(true);
    CScript scriptPubKey = CScript() <<  ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;
    int nBlocks = Params().GetConsensus().nCoinbaseMaturity + 1;
    for (int i = 0; i < nBlocks; i++)
    {
        std::vector<CMutableTransaction> noTxns;
        CBlock b = CreateAndProcessBlock(noTxns, scriptPubKey);
        coinbaseTxns.push_back(b.vtx[0]);
    }
}

//
// Create a new block with just given transactions, coinbase paying to
// scriptPubKey, and try to add it to the current chain.
//
CBlock
TestChain100Setup::CreateAndProcessBlock(const std::vector<CMutableTransaction>& txns, const CScript& scriptPubKey)
{
    const CChainParams& chainparams = Params();
    CBlockTemplate *pblocktemplate = BlockAssembler(chainparams).CreateNewBlock(scriptPubKey);
    CBlock& block = pblocktemplate->block;

    // Replace mempool-selected txns with just coinbase plus passed-in txns:
    block.vtx.resize(1);
    BOOST_FOREACH(const CMutableTransaction& tx, txns)
        block.vtx.push_back(tx);
    // The coinbase was built claiming the template txs' fees; since we just
    // dropped them, reset its value to the plain subsidy or the block will
    // be rejected with "coinbase pays too much".
    if (!block.vtx[0].vout.empty()) {
        CMutableTransaction coinbase(block.vtx[0]);
        coinbase.vout[0].nValue = GetProofOfWorkSubsidy();
        block.vtx[0] = coinbase;
    }
    // IncrementExtraNonce creates a valid coinbase and merkleRoot
    unsigned int extraNonce = 0;
    IncrementExtraNonce(&block, chainActive.Tip(), extraNonce);

    // This chain uses scrypt for proof of work: mine with GetPoWHash or the
    // block will be rejected as "high-hash" by ContextualCheckBlock.
    // Mine in parallel: a single thread needs ~65k scrypt hashes per block
    // at regtest difficulty, far too slow for a unit-test fixture. Threads
    // scan interleaved nonce values; the first valid hash wins.
    {
        std::atomic<bool> fFound(false);
        std::atomic<uint32_t> nWinner(0);
        unsigned int nThreads = std::max(1u, std::min(16u, std::thread::hardware_concurrency()));
        boost::thread_group group;
        for (unsigned int t = 0; t < nThreads; t++) {
            group.create_thread([&block, &chainparams, &fFound, &nWinner, t, nThreads]() {
                CBlockHeader hdr = block; // thread-local copy to hash
                for (uint32_t nNonce = t; !fFound.load(std::memory_order_relaxed); nNonce += nThreads) {
                    hdr.nNonce = nNonce;
                    if (CheckProofOfWork(hdr.GetPoWHash(), hdr.nBits, chainparams.GetConsensus())) {
                        if (!fFound.exchange(true))
                            nWinner = nNonce;
                        return;
                    }
                }
            });
        }
        group.join_all();
        block.nNonce = nWinner;
    }

    CValidationState state;
    if (!ProcessNewBlock(state, chainparams, NULL, &block, true, NULL, false)) {
        fprintf(stderr, "TestChain100Setup: block rejected: %s (debug: %s) time=%u nBits=%08x\n",
                state.GetRejectReason().c_str(), state.GetDebugMessage().c_str(), block.nTime, block.nBits);
    }

    CBlock result = block;
    delete pblocktemplate;
    return result;
}

TestChain100Setup::~TestChain100Setup()
{
}


CTxMemPoolEntry TestMemPoolEntryHelper::FromTx(CMutableTransaction &tx, CTxMemPool *pool) {
    CTransaction txn(tx);
    return FromTx(txn, pool);
}

CTxMemPoolEntry TestMemPoolEntryHelper::FromTx(CTransaction &txn, CTxMemPool *pool) {
    bool hasNoDependencies = pool ? pool->HasNoInputsOf(txn) : hadNoDependencies;
    // Hack to assume either its completely dependent on other mempool txs or not at all
    CAmount inChainValue = hasNoDependencies ? txn.GetValueOut() : 0;

    return CTxMemPoolEntry(txn, nFee, nTime, dPriority, nHeight,
                           hasNoDependencies, inChainValue, spendsCoinbase, sigOpCost, lp);
}

void Shutdown(void* parg)
{
  exit(0);
}

void StartShutdown()
{
  exit(0);
}

bool ShutdownRequested()
{
  return false;
}
