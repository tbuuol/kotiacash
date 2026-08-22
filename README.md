# Kotia Cash (KOT)

**Kotia Cash** is a hybrid **PoW (scrypt) + PoS** blockchain with an
integrated Qt wallet and **client-side CoinJoin mixing** for transaction
privacy.

Based on Bitcoin Core 0.13.2/0.14 code (Novacoin/PPC lineage for the
Proof-of-Stake part), with **cashaddr** encoding, **stealth addresses** and
a native CoinJoin engine built into the wallet.

---

## What it is and what it does

| Feature | Details |
|---|---|
| **Consensus** | PoW scrypt up to block 1,000,000, then pure Proof-of-Stake |
| **Rewards** | PoW: 100 KOT/block (block 1: 100,000 KOT) · PoS: 20 KOT/stake |
| **Block time** | 2 minutes (retarget every 10 min) |
| **Maturity** | coinbase: 20 blocks · stake: min 12 hours |
| **Ports** | P2P `46174` · RPC `46173` (testnet `56174`/`56173`) |
| **Addresses** | cashaddr `kotia:…` (native prefix) + legacy base58 (46/45) |
| **WIF** | prefix 173 · standard BIP32 xprv/xpub |
| **Genesis** | `000008e4586479cd9dc5832b262414a60f152381e371cf7940ffe9f5aa4b80a0` |
| **DNS seeds** | `node1.kotia.cash`, `node2.kotia.cash`, `secondnode.kotia.cash` |

### Privacy: CoinJoin

The wallet ships a **client-side** mixing engine (inspired by Dash
PrivateSend / Wasabi): it splits amounts into **randomly-sized** outputs
(the "denomination" is a cap, not a fixed amount), breaking the
"same amount = same owner" heuristic used by chain analysis.

- Denominations: 0.001 · 0.01 · 0.1 · 1 · 10 KOT
- Configurable remix rounds (default 4) — each round inherits the rounds
  of its inputs
- Per-outpoint rounds tracking, mixing statistics, mempool-chain limits
  with automatic confirmation waiting
- No coordinator required: the transaction is built, signed and
  broadcast entirely by the wallet

**Qt interface**: dedicated CoinJoin page (amount, max-tx cap, rounds,
per-denomination progress, live status, stop button). **RPC**:
`coinjoininfo`, `coinjoinmix <denom> [rounds] [amount]`,
`setcoinjoinenabled`.

### Other wallet features

- **Stealth addresses** (`sm…` prefix): one-time non-reusable addresses
- Integrated **staking** (checkbox + weight/expected time in the GUI)
- BIP32 (HD), multisig, coin control, message sign/verify
- DNS seeds + checkpoints for fast synchronization

---

## Prebuilt binaries (`releases/` folder)

All binaries in `releases/` are built from this source tree and include
the August 2026 audit fixes (see the [changelog](#technical-changelog)
below).

| File | System | Contents |
|---|---|---|
| `releases/windows/kotia-windows-coinjoin.zip` | Windows x64 | `kotia-qt.exe`, `kotiad.exe`, `kotia-cli.exe`, `kotia-tx.exe` |
| `releases/linux/kotia-linux-portable.tar.gz` | Linux x64 | portable: binaries + bundled Qt/boost libraries |
| `releases/macos/kotia-wallet-macos-x86_64.tar.gz` | macOS 10.12+ (x86_64) | `kotia-qt` (static, dependencies bundled) |

Quick start (Linux): `tar xf kotia-linux-portable.tar.gz && ./kotia-qt.sh`
<br>macOS: unpack and run `kotia-qt` (on first launch confirm via
right-click → Open).

To build from source see [Building](#building).

---

## Building

```bash
./autogen.sh
./configure            # options below
make -j$(nproc)
```

Useful options: `--with-gui=qt5` (default) · `--without-gui` (node only) ·
`--disable-tests` · `--with-incompatible-bdb` (BDB 5.3+) · `--disable-bench`

Cross-compiling with the `depends/` system:

```bash
cd depends && make HOST=x86_64-w64-mingw32 NO_QT=1   # Windows daemon only
cd .. && CONFIG_SITE=$PWD/depends/x86_64-w64-mingw32/share/config.site \
       ./configure --prefix=/ && make -j$(nproc)
```

Native dependencies: g++ ≥5, Qt5, Boost, OpenSSL, libevent, Berkeley DB 5.3
(`libdb5.3++-dev`), miniupnpc (optional).

### Tests

```bash
make -C src -j$(nproc) test/test_bitcoin
./src/test/test_bitcoin --run_test=coinjoin_tests   # CoinJoin suite
./src/test/test_bitcoin                             # full module
```

---

## CoinJoin RPC — examples

```bash
# status and progress
kotia-cli coinjoininfo

# mix 10 KOT with transactions capped at 1 KOT, 4 rounds
kotia-cli coinjoinmix 1 4 10

# enable automatic mixing (persisted in the wallet)
kotia-cli setcoinjoinenabled true
```

---

## Repository layout

```
src/coinjoin.{h,cpp}        CoinJoin engine (sessions, AutoMix, splitter)
src/qt/coinjoinpage.*       CoinJoin GUI page
src/qt/coinjoinworker.*     background mixing worker (non-blocking UI)
src/wallet/, src/           wallet + node (Bitcoin Core 0.13/0.14 lineage)
src/test/coinjoin_tests.cpp CoinJoin test suite (unit + e2e on regtest)
contrib/fix_bloom_testdata.py  test-vector regeneration tool for forked chains
doc/coinjoin-audit-2026-08-22.md  full CoinJoin wallet audit report
releases/                   prebuilt binaries for Windows/Linux/macOS
```

## Technical changelog

### v2.4 (2026-08-22) — CoinJoin audit
- **RPC deadlock fix**: `coinjoinmix` no longer holds `cs_main`/`cs_wallet`
  for the whole mix (the node used to freeze for the duration of the run)
- **Data race fix**: status/flag/stats are now atomic and lock-guarded
  (GUI/RPC/worker thread safety)
- **Lock-order inversion fixed** in sign/broadcast and coin selection
- **Dust output fix**: minimum mixed output raised above the chain's dust
  threshold (minRelayTxFee = 10000 sat/kB)
- **Fee fix**: 200 B/input + 10% estimate (eliminates "insufficient
  priority" rejections for >1KB mixing transactions)
- **Splitter**: `SplitAmountsForMixing()` — pure and unit-tested; the last
  output can no longer exceed the denomination cap
- Fixed `chunkIndex` underflow on the mempool-chain retry; guards against
  division-by-zero and negative amounts; AutoMix re-entrancy guard
- The `amount` parameter of `coinjoinmix` is now honored; rounds validated 1–100
- CoinJoin settings persisted to and reloaded from the wallet DB
- In-memory wallet keypool no longer attempts to open BerkeleyDB
- Qt: removed dead status timer in the worker
- **New test suite** `coinjoin_tests` (9 cases: unit + full e2e on regtest
  with a real wallet) and repair of the chain's test infrastructure (scrypt
  PoW in the fixture, vectors regenerated for the chain formats) — see
  `doc/coinjoin-audit-2026-08-22.md`

### History (from the original fork)
- v2.13.2.x: fake-stake fix, mempool dust mitigation, BDB 6.2.38, OpenSSL 1.0.2u
- v2.13.2.1: Bitcoin Core 0.13.2 base + 0.14.x backports
- v2.3: CoinJoin engine (sessions, multi-round AutoMix, async worker)
- v2.2: mixing fee-calc fix

## License

MIT — see `COPYING`. Derived from Bitcoin Core (MIT), with Novacoin/PPC
components (MIT) for the Proof-of-Stake part.
