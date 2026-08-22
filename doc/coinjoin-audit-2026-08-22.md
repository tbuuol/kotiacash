# Kotia CoinJoin — Debug / Test / Improvement Report

Date: 2026-08-22
Scope: full audit of the CoinJoin wallet feature plus the unit-test
infrastructure it depends on.

## 1. CoinJoin core fixes (src/coinjoin.{h,cpp})

* **RPC deadlock / node freeze**: `coinjoinmix` held `cs_main` + `cs_wallet`
  across the whole (minutes-long) `AutoMix` run. Both are recursive here so it
  did not self-deadlock, but it froze the entire node. The RPC now snapshots
  parameters, releases the locks for the mixing, and re-acquires them only to
  build the response.
* **Data races**: `fMixing`, `strLastStatus`, the statistics counters and the
  requested/completed counters were plain fields written from the Qt worker
  thread and read from the GUI/RPC threads (torn `std::string` reads can
  crash). `fMixing` is now `std::atomic`, every getter returns a copy under
  `cs_coinjoin`, and status updates go through `SetStatus()`.
* **Lock-order inversion**: `SignAndBroadcast` held `cs_coinjoin` across
  `AcceptToMemoryPool` (which takes `cs_main`). It now copies the session,
  releases the lock, then signs/broadcasts under `cs_main` only.
  `SelectCoinsForDenomination` no longer holds `cs_coinjoin` while calling
  `GetOutpointRounds()` (which locks it).
* **Dust outputs**: mixed outputs could be as small as 1000 sat, below this
  fork's dust threshold (minRelayTxFee is 10000 sat/kB → 5460-sat threshold
  for P2PKH). Rejected with "dust" at broadcast. `COINJOIN_MIN_MIX_OUTPUT`
  raised to 10000.
* **"insufficient priority"**: the flat 10000-sat fee undershot
  `minRelayTxFee` for transactions over ~1 KB (the size estimate used
  150 B/input; real P2PKH inputs are ~180 B). The estimate now budgets
  200 B/input + 10% margin.
* **Splitter correctness**: `SplitAmountsForMixing()` (extracted as a pure,
  unit-testable static) fixes the old bug where the last output could exceed
  the denomination cap (e.g. 3000 split as 1000+2000 with max 1500). It now
  reserves tail room and guarantees: exact sum, ≥2 outputs, every output in
  [10000, maxOutputAmount].
* **Chunk-retry underflow**: the mempool-chain-limit retry did
  `chunkIndex--` on a `size_t`; at chunk 0 it wrapped and silently dropped
  the remainder of the round. Loop index is signed now.
* **Parameter guards**: `CreateCoinJoinTransaction` rejects
  `denomination <= dust+fee` (was a division-by-zero risk via RPC) and
  negative mix amounts; `GetMixingProgress` guards `denomination == 0`.
* **Re-entrancy guard**: `AutoMix` refuses to start while another mix is
  running; an RAII guard clears the mixing flag on every exit path.
* **Wallet snapshot before signing**: input scripts/values are copied under
  `cs_wallet` instead of being read from `mapWallet` concurrently with
  `AddToWallet`.
* **Session hygiene**: expired sessions are refused by `JoinSession`;
  zero-activity sessions are no longer instantly "expired"; `AutoMix` also
  purges stale sessions at start.
* **Ignored RPC parameter**: `coinjoinmix <denom> <rounds> <amount>` now
  actually passes `amount` through, validates `rounds` (1..100) and reports
  `transactions_requested` / `rounds_completed` / `last_status`.

## 2. Wallet integration fixes

* `setcoinjoinenabled` / Qt settings are now persisted **and loaded**
  (`CWallet::LoadWallet` reads `cjconfig` records; `SetCoinJoinEnabled` /
  `SetCoinJoinRounds` write through to the wallet DB).
* `TopUpKeyPool` / `ReserveKeyFromKeyPool` no longer open a BerkeleyDB file
  for in-memory (`!fFileBacked`) wallets — `GetKeyFromPool()` falls back to
  `GenerateNewKey()` cleanly (previously threw `CDB: Error 2`).
* Boost ≥1.75 compatibility: `boost::filesystem::basename/extension` and
  `copy_option` replaced with portable equivalents (`#if BOOST_VERSION`
  guarded for `copy_options`).

## 3. Qt fixes

* `CoinJoinWorker::doWork` had a status `QTimer` that could never fire
  (the blocking AutoMix occupied the thread's event loop). Removed; the page
  already polls the now thread-safe `GetLastStatus()`.
* Minimum-denomination check in the GUI uses `2*COINJOIN_MIN_MIX_OUTPUT +
  fee` instead of the old hardcoded 2000.

## 4. New CoinJoin test suite (src/test/coinjoin_tests.cpp)

Pure-logic tests (no chain needed, fast):
* denomination helpers, session lifecycle/expiry/cleanup, participant cap,
* `CoinJoinSession` serialization round-trip,
* splitter invariants incl. 500-iteration random stress,
* outpoint rounds tracking.

Integration test (regtest chain + real in-memory wallet):
* funded wallet from matured coinbases, denomination suggestion,
* single CoinJoin tx creation: fee accounting, output bounds, invalid params,
* full `AutoMix` run (2 rounds × 3 chunks): tx count, mempool contents,
  statistics, rounds tracking, block confirmation, progress reporting,
* stop-mid-mix behavior and mixing-flag reset,
* concurrent status reads from a second thread (regression test for the
  `std::string` race),
* invalid parameter handling.

## 5. Test-infrastructure repairs (benefit every suite)

* `TestChain100Setup` never worked on this chain: it mined with
  `GetHash()` (double-SHA256) while consensus checks `GetPoWHash()` (scrypt)
  → every block was "high-hash"-rejected (chain height stayed 0).
  Fixed + PoW search is now multi-threaded (a serial scrypt search needs
  ~65k hashes/block).
* Fixture now mines `nCoinbaseMaturity + 1` blocks so the first coinbase is
  actually spendable, and resets the coinbase value after replacing the
  template's transactions ("coinbase pays too much" rejections).
* `CDataStream`-based legacy test vectors repaired for this chain's
  transaction/CCoins formats (`nTime` field, `vchBlockSig`):
  bloom_tests (full regeneration incl. merkle roots and within-block
  prevouts — see `contrib/fix_bloom_testdata.py`), coins_tests
  (`ccoins_serialization` rewritten as round-trip), key_tests, hash_tests,
  util_tests, rpc_tests.
* Fork-specific encoding vectors fixed: cashaddr checksums (the fork tweaked
  the BCH checksum constant `0x1e4f43e870` → `0x1e4f43e470`), base58
  address/WIF vectors regenerated for prefixes 46/45/173, dstencode tests.
* wallet_crypto: OpenSSL 3.x zeroes the final AES block on invalid padding;
  comparison now accounts for it (the test already had a 1.0.1j variant of
  the same workaround).
* `coinjoin_tests` wired into `src/Makefile.test.include`.

Result: the suites listed above went from **not runnable at all** (an
early abort cascaded ~165 SIGABRT failures through the module) to fully
green, including the new CoinJoin coverage.

## 6. Known remaining failures (pre-existing, out of CoinJoin scope)

| Suite | Count | Root cause |
|---|---|---|
| versionbits_tests | 2008 | fork uses `GetPastTimeLimit()` (block time) instead of `GetMedianTimePast()` in BIP9 state machine; vectors assume Bitcoin timing |
| miner_tests | 221 | hardcoded Bitcoin nonces (sha256 PoW), 50 BTC subsidy assumption vs 100 KOT, PoS template differences |
| pow_tests | 4 | retarget expected values are Bitcoin's; fork retarget formula differs |
| mempool_tests (ancestor index) | 4 | sort tie-break divergence for equal-fee entries |
| tx_validationcache_tests | 4 | legacy tx-format vectors |
| rpc_wallet_tests | 6 | account/address RPC handling diverges (cashaddr-only decode in some paths) |
| rpc_parse_monetary_values, rpc_ban | 2 | `AmountFromValue` gained `allowZero`; `setban` semantics differ |

## 7. How to build & run the tests natively (Linux)

```bash
mkdir build-native && cd build-native
../configure --with-gui=qt5 --with-incompatible-bdb --disable-bench \
    --without-libzmq CPPFLAGS="-std=c++14-friendly flags if needed"
make -C src -j$(nproc) test/test_bitcoin
./src/test/test_bitcoin --run_test=coinjoin_tests     # CoinJoin suite
./src/test/test_bitcoin                               # full module
```

Note: the tree pins `-std=c++11`; a local test build against very new Boost
(≥1.86) additionally needs the two-line `configure.ac` change to
`AX_CXX_COMPILE_STDCXX([14], ...)` plus `boost/filesystem`/`copy_option`
fixes that are already included in this tree.
