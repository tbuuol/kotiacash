// Copyright (c) 2014-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "coins.h"
#include "random.h"
#include "script/standard.h"
#include "uint256.h"
#include "utilstrencodings.h"
#include "test/test_bitcoin.h"
#include "main.h"
#include "consensus/validation.h"

#include <vector>
#include <map>

#include <boost/test/unit_test.hpp>

namespace
{
class CCoinsViewTest : public CCoinsView
{
    uint256 hashBestBlock_;
    std::map<uint256, CCoins> map_;

public:
    bool GetCoins(const uint256& txid, CCoins& coins) const
    {
        std::map<uint256, CCoins>::const_iterator it = map_.find(txid);
        if (it == map_.end()) {
            return false;
        }
        coins = it->second;
        if (coins.IsPruned() && insecure_rand() % 2 == 0) {
            // Randomly return false in case of an empty entry.
            return false;
        }
        return true;
    }

    bool HaveCoins(const uint256& txid) const
    {
        CCoins coins;
        return GetCoins(txid, coins);
    }

    uint256 GetBestBlock() const { return hashBestBlock_; }

    bool BatchWrite(CCoinsMap& mapCoins, const uint256& hashBlock)
    {
        for (CCoinsMap::iterator it = mapCoins.begin(); it != mapCoins.end(); ) {
            if (it->second.flags & CCoinsCacheEntry::DIRTY) {
                // Same optimization used in CCoinsViewDB is to only write dirty entries.
                map_[it->first] = it->second.coins;
                if (it->second.coins.IsPruned() && insecure_rand() % 3 == 0) {
                    // Randomly delete empty entries on write.
                    map_.erase(it->first);
                }
            }
            mapCoins.erase(it++);
        }
        if (!hashBlock.IsNull())
            hashBestBlock_ = hashBlock;
        return true;
    }
};

class CCoinsViewCacheTest : public CCoinsViewCache
{
public:
    CCoinsViewCacheTest(CCoinsView* base) : CCoinsViewCache(base) {}

    void SelfTest() const
    {
        // Manually recompute the dynamic usage of the whole data, and compare it.
        size_t ret = memusage::DynamicUsage(cacheCoins);
        for (CCoinsMap::iterator it = cacheCoins.begin(); it != cacheCoins.end(); it++) {
            ret += it->second.coins.DynamicMemoryUsage();
        }
        BOOST_CHECK_EQUAL(DynamicMemoryUsage(), ret);
    }

};

}

BOOST_FIXTURE_TEST_SUITE(coins_tests, BasicTestingSetup)

static const unsigned int NUM_SIMULATION_ITERATIONS = 40000;

// This is a large randomized insert/remove simulation test on a variable-size
// stack of caches on top of CCoinsViewTest.
//
// It will randomly create/update/delete CCoins entries to a tip of caches, with
// txids picked from a limited list of random 256-bit hashes. Occasionally, a
// new tip is added to the stack of caches, or the tip is flushed and removed.
//
// During the process, booleans are kept to make sure that the randomized
// operation hits all branches.
BOOST_AUTO_TEST_CASE(coins_cache_simulation_test)
{
    // Various coverage trackers.
    bool removed_all_caches = false;
    bool reached_4_caches = false;
    bool added_an_entry = false;
    bool removed_an_entry = false;
    bool updated_an_entry = false;
    bool found_an_entry = false;
    bool missed_an_entry = false;

    // A simple map to track what we expect the cache stack to represent.
    std::map<uint256, CCoins> result;

    // The cache stack.
    CCoinsViewTest base; // A CCoinsViewTest at the bottom.
    std::vector<CCoinsViewCacheTest*> stack; // A stack of CCoinsViewCaches on top.
    stack.push_back(new CCoinsViewCacheTest(&base)); // Start with one cache.

    // Use a limited set of random transaction ids, so we do test overwriting entries.
    std::vector<uint256> txids;
    txids.resize(NUM_SIMULATION_ITERATIONS / 8);
    for (unsigned int i = 0; i < txids.size(); i++) {
        txids[i] = GetRandHash();
    }

    for (unsigned int i = 0; i < NUM_SIMULATION_ITERATIONS; i++) {
        // Do a random modification.
        {
            uint256 txid = txids[insecure_rand() % txids.size()]; // txid we're going to modify in this iteration.
            CCoins& coins = result[txid];
            CCoinsModifier entry = stack.back()->ModifyCoins(txid);
            BOOST_CHECK(coins == *entry);
            if (insecure_rand() % 5 == 0 || coins.IsPruned()) {
                if (coins.IsPruned()) {
                    added_an_entry = true;
                } else {
                    updated_an_entry = true;
                }
                coins.nVersion = insecure_rand();
                coins.vout.resize(1);
                coins.vout[0].nValue = insecure_rand();
                *entry = coins;
            } else {
                coins.Clear();
                entry->Clear();
                removed_an_entry = true;
            }
        }

        // Once every 1000 iterations and at the end, verify the full cache.
        if (insecure_rand() % 1000 == 1 || i == NUM_SIMULATION_ITERATIONS - 1) {
            for (std::map<uint256, CCoins>::iterator it = result.begin(); it != result.end(); it++) {
                const CCoins* coins = stack.back()->AccessCoins(it->first);
                if (coins) {
                    BOOST_CHECK(*coins == it->second);
                    found_an_entry = true;
                } else {
                    BOOST_CHECK(it->second.IsPruned());
                    missed_an_entry = true;
                }
            }
            BOOST_FOREACH(const CCoinsViewCacheTest *test, stack) {
                test->SelfTest();
            }
        }

        if (insecure_rand() % 100 == 0) {
            // Every 100 iterations, flush an intermediate cache
            if (stack.size() > 1 && insecure_rand() % 2 == 0) {
                unsigned int flushIndex = insecure_rand() % (stack.size() - 1);
                stack[flushIndex]->Flush();
            }
        }
        if (insecure_rand() % 100 == 0) {
            // Every 100 iterations, change the cache stack.
            if (stack.size() > 0 && insecure_rand() % 2 == 0) {
                //Remove the top cache
                stack.back()->Flush();
                delete stack.back();
                stack.pop_back();
            }
            if (stack.size() == 0 || (stack.size() < 4 && insecure_rand() % 2)) {
                //Add a new cache
                CCoinsView* tip = &base;
                if (stack.size() > 0) {
                    tip = stack.back();
                } else {
                    removed_all_caches = true;
                }
                stack.push_back(new CCoinsViewCacheTest(tip));
                if (stack.size() == 4) {
                    reached_4_caches = true;
                }
            }
        }
    }

    // Clean up the stack.
    while (stack.size() > 0) {
        delete stack.back();
        stack.pop_back();
    }

    // Verify coverage.
    BOOST_CHECK(removed_all_caches);
    BOOST_CHECK(reached_4_caches);
    BOOST_CHECK(added_an_entry);
    BOOST_CHECK(removed_an_entry);
    BOOST_CHECK(updated_an_entry);
    BOOST_CHECK(found_an_entry);
    BOOST_CHECK(missed_an_entry);
}

// This test is similar to the previous test
// except the emphasis is on testing the functionality of UpdateCoins
// random txs are created and UpdateCoins is used to update the cache stack
// In particular it is tested that spending a duplicate coinbase tx
// has the expected effect (the other duplicate is overwitten at all cache levels)
BOOST_AUTO_TEST_CASE(updatecoins_simulation_test)
{
    bool spent_a_duplicate_coinbase = false;
    // A simple map to track what we expect the cache stack to represent.
    std::map<uint256, CCoins> result;

    // The cache stack.
    CCoinsViewTest base; // A CCoinsViewTest at the bottom.
    std::vector<CCoinsViewCacheTest*> stack; // A stack of CCoinsViewCaches on top.
    stack.push_back(new CCoinsViewCacheTest(&base)); // Start with one cache.

    // Track the txids we've used and whether they have been spent or not
    std::map<uint256, CAmount> coinbaseids;
    std::set<uint256> alltxids;
    std::set<uint256> duplicateids;

    for (unsigned int i = 0; i < NUM_SIMULATION_ITERATIONS; i++) {
        {
            CMutableTransaction tx;
            tx.vin.resize(1);
            tx.vout.resize(1);
            tx.vout[0].nValue = i; //Keep txs unique unless intended to duplicate
            // A spendable script is required: CCoins::FromTx() calls
            // ClearUnspendable(), and an empty scriptPubKey would prune the
            // output, making the simulated later spend hit an assert.
            tx.vout[0].scriptPubKey = CScript() << OP_TRUE;
            unsigned int height = insecure_rand();

            // 1/10 times create a coinbase
            if (insecure_rand() % 10 == 0 || coinbaseids.size() < 10) {
                // 1/100 times create a duplicate coinbase
                if (insecure_rand() % 10 == 0 && coinbaseids.size()) {
                    std::map<uint256, CAmount>::iterator coinbaseIt = coinbaseids.lower_bound(GetRandHash());
                    if (coinbaseIt == coinbaseids.end()) {
                        coinbaseIt = coinbaseids.begin();
                    }
                    //Use same random value to have same hash and be a true duplicate
                    tx.vout[0].nValue = coinbaseIt->second;
                    assert(tx.GetHash() == coinbaseIt->first);
                    duplicateids.insert(coinbaseIt->first);
                }
                else {
                    coinbaseids[tx.GetHash()] = tx.vout[0].nValue;
                }
                assert(CTransaction(tx).IsCoinBase());
            }
            // 9/10 times create a regular tx
            else {
                uint256 prevouthash;
                // equally likely to spend coinbase or non coinbase
                std::set<uint256>::iterator txIt = alltxids.lower_bound(GetRandHash());
                if (txIt == alltxids.end()) {
                    txIt = alltxids.begin();
                }
                prevouthash = *txIt;

                // Construct the tx to spend the coins of prevouthash
                tx.vin[0].prevout.hash = prevouthash;
                tx.vin[0].prevout.n = 0;

                // Update the expected result of prevouthash to know these coins are spent
                CCoins& oldcoins = result[prevouthash];
                oldcoins.Clear();

                // It is of particular importance here that once we spend a coinbase tx hash
                // it is no longer available to be duplicated (or spent again)
                // BIP 34 in conjunction with enforcing BIP 30 (at least until BIP 34 was active)
                // results in the fact that no coinbases were duplicated after they were already spent
                alltxids.erase(prevouthash);
                coinbaseids.erase(prevouthash);

                // The test is designed to ensure spending a duplicate coinbase will work properly
                // if that ever happens and not resurrect the previously overwritten coinbase
                if (duplicateids.count(prevouthash))
                    spent_a_duplicate_coinbase = true;

                assert(!CTransaction(tx).IsCoinBase());
            }
            // Track this tx to possibly spend later
            alltxids.insert(tx.GetHash());

            // Update the expected result to know about the new output coins
            CCoins &coins = result[tx.GetHash()];
            coins.FromTx(tx, height);

            UpdateCoins(tx, *(stack.back()), height);
        }

        // Once every 1000 iterations and at the end, verify the full cache.
        if (insecure_rand() % 1000 == 1 || i == NUM_SIMULATION_ITERATIONS - 1) {
            for (std::map<uint256, CCoins>::iterator it = result.begin(); it != result.end(); it++) {
                const CCoins* coins = stack.back()->AccessCoins(it->first);
                if (coins) {
                    BOOST_CHECK(*coins == it->second);
                 } else {
                    BOOST_CHECK(it->second.IsPruned());
                 }
            }
        }

        if (insecure_rand() % 100 == 0) {
            // Every 100 iterations, flush an intermediate cache
            if (stack.size() > 1 && insecure_rand() % 2 == 0) {
                unsigned int flushIndex = insecure_rand() % (stack.size() - 1);
                stack[flushIndex]->Flush();
            }
        }
        if (insecure_rand() % 100 == 0) {
            // Every 100 iterations, change the cache stack.
            if (stack.size() > 0 && insecure_rand() % 2 == 0) {
                stack.back()->Flush();
                delete stack.back();
                stack.pop_back();
            }
            if (stack.size() == 0 || (stack.size() < 4 && insecure_rand() % 2)) {
                CCoinsView* tip = &base;
                if (stack.size() > 0) {
                    tip = stack.back();
                }
                stack.push_back(new CCoinsViewCacheTest(tip));
           }
        }
    }

    // Clean up the stack.
    while (stack.size() > 0) {
        delete stack.back();
        stack.pop_back();
    }

    // Verify coverage.
    BOOST_CHECK(spent_a_duplicate_coinbase);
}

BOOST_AUTO_TEST_CASE(ccoins_serialization)
{
    // The historical hex vectors from Bitcoin Core predate this chain's
    // CCoins format (which adds fCoinStake in the header code and a
    // trailing nTime), so the vectors are constructed and round-tripped
    // through the actual serializer instead.

    // Round trip a regular coins object
    CCoins cc1;
    cc1.fCoinBase = false;
    cc1.nHeight = 203998;
    cc1.nVersion = 1;
    cc1.nTime = 1000;
    cc1.vout.resize(2);
    cc1.vout[0].SetNull();
    cc1.vout[1].nValue = 60000000000ULL;
    cc1.vout[1].scriptPubKey = GetScriptForDestination(CKeyID(uint160(ParseHex("816115944e077fe7c803cfa57f29b36bf87c1d35"))));

    CDataStream ss1(SER_DISK, CLIENT_VERSION);
    ss1 << cc1;
    CCoins cc1b;
    ss1 >> cc1b;
    BOOST_CHECK_EQUAL(cc1b.nVersion, 1);
    BOOST_CHECK_EQUAL(cc1b.fCoinBase, false);
    BOOST_CHECK_EQUAL(cc1b.fCoinStake, false);
    BOOST_CHECK_EQUAL(cc1b.nHeight, 203998);
    BOOST_CHECK_EQUAL(cc1b.nTime, 1000);
    BOOST_CHECK_EQUAL(cc1b.vout.size(), 2);
    BOOST_CHECK_EQUAL(cc1b.IsAvailable(0), false);
    BOOST_CHECK_EQUAL(cc1b.IsAvailable(1), true);
    BOOST_CHECK_EQUAL(cc1b.vout[1].nValue, 60000000000ULL);
    BOOST_CHECK_EQUAL(HexStr(cc1b.vout[1].scriptPubKey), HexStr(cc1.vout[1].scriptPubKey));
    BOOST_CHECK(cc1 == cc1b);

    // Round trip a coinbase with many outputs, some spent
    CCoins cc2;
    cc2.fCoinBase = true;
    cc2.fCoinStake = false;
    cc2.nHeight = 120891;
    cc2.nVersion = 1;
    cc2.nTime = 123456789;
    cc2.vout.resize(17);
    for (int i = 0; i < 17; i++)
        cc2.vout[i].SetNull();
    cc2.vout[4].nValue = 234925952;
    cc2.vout[4].scriptPubKey = GetScriptForDestination(CKeyID(uint160(ParseHex("61b01caab50f1b8e9c50a5057eb43c2d9563a4ee"))));
    cc2.vout[16].nValue = 110397;
    cc2.vout[16].scriptPubKey = GetScriptForDestination(CKeyID(uint160(ParseHex("8c988f1a4a4de2161e0f50aac7f17e7f9555caa4"))));

    CDataStream ss2(SER_DISK, CLIENT_VERSION);
    ss2 << cc2;
    CCoins cc2b;
    ss2 >> cc2b;
    BOOST_CHECK_EQUAL(cc2b.nVersion, 1);
    BOOST_CHECK_EQUAL(cc2b.fCoinBase, true);
    BOOST_CHECK_EQUAL(cc2b.nHeight, 120891);
    BOOST_CHECK_EQUAL(cc2b.nTime, 123456789);
    BOOST_CHECK_EQUAL(cc2b.vout.size(), 17);
    for (int i = 0; i < 17; i++) {
        BOOST_CHECK_EQUAL(cc2b.IsAvailable(i), i == 4 || i == 16);
    }
    BOOST_CHECK_EQUAL(cc2b.vout[4].nValue, 234925952);
    BOOST_CHECK_EQUAL(cc2b.vout[16].nValue, 110397);
    BOOST_CHECK(cc2 == cc2b);

    // Coinstake round trip exercises the header-code stake bit
    CCoins cc3;
    cc3.fCoinStake = true;
    cc3.nHeight = 555555;
    cc3.nVersion = 2;
    cc3.nTime = 42;
    cc3.vout.resize(3);
    cc3.vout[2].nValue = 777;
    cc3.vout[2].scriptPubKey << OP_TRUE;
    CDataStream ss3(SER_DISK, CLIENT_VERSION);
    ss3 << cc3;
    CCoins cc3b;
    ss3 >> cc3b;
    BOOST_CHECK_EQUAL(cc3b.fCoinStake, true);
    BOOST_CHECK_EQUAL(cc3b.nHeight, 555555);
    BOOST_CHECK_EQUAL(cc3b.nTime, 42);
    BOOST_CHECK_EQUAL(cc3b.IsAvailable(2), true);
    BOOST_CHECK(cc3 == cc3b);

    // Smallest possible example: a fully-spent CCoins cannot be serialized
    // (the serializer asserts); an empty stream represents it.
    CCoins cc4;
    BOOST_CHECK(cc4.IsPruned());
    CDataStream ssx(SER_DISK, CLIENT_VERSION);
    BOOST_CHECK_EQUAL(HexStr(ssx.begin(), ssx.end()), "");

    // Truncated streams must throw, not crash:
    // scriptPubKey that ends beyond the end of the stream
    CDataStream ss4(ParseHex("0002000800"), SER_DISK, CLIENT_VERSION);
    try {
        CCoins cc5;
        ss4 >> cc5;
        BOOST_CHECK_MESSAGE(false, "We should have thrown");
    } catch (const std::ios_base::failure& e) {
    }

    // Very large scriptPubKey (3*10^9 bytes) past the end of the stream
    CDataStream tmp(SER_DISK, CLIENT_VERSION);
    uint64_t x = 3000000000ULL;
    tmp << VARINT(x);
    BOOST_CHECK_EQUAL(HexStr(tmp.begin(), tmp.end()), "8a95c0bb00");
    CDataStream ss5(ParseHex("0002008a95c0bb0000"), SER_DISK, CLIENT_VERSION);
    try {
        CCoins cc6;
        ss5 >> cc6;
        BOOST_CHECK_MESSAGE(false, "We should have thrown");
    } catch (const std::ios_base::failure& e) {
    }
}


BOOST_AUTO_TEST_SUITE_END()