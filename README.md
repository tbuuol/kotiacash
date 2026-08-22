# Kotia Cash (KOT)

**Kotia Cash** è una blockchain ibrida **PoW (scrypt) + PoS** con portafoglio
Qt integrato e **mixing CoinJoin client-side** per la privacy delle
transazioni.

Basata sul codice Bitcoin Core 0.13.2/0.14 (lineage Novacoin/PPC per la
parte Proof-of-Stake), con encoding **cashaddr**, indirizzi **stealth** e un
motore CoinJoin nativo nel wallet.

---

## Cos'è e cosa fa

| Funzione | Descrizione |
|---|---|
| **Consenso** | PoW scrypt fino al blocco 1.000.000, poi Proof-of-Stake puro |
| **Ricompense** | PoW: 100 KOT/blocco (blocco 1: 100.000 KOT) · PoS: 20 KOT/stake |
| **Tempo blocco** | 2 minuti (retarget ogni 10 min) |
| **Maturità** | coinbase: 20 blocchi · stake: min 12 ore |
| **Porte** | P2P `46174` · RPC `46173` (testnet `56174`/`56173`) |
| **Indirizzi** | cashaddr `kotia:…` (prefisso nativo) + base58 legacy (46/45) |
| **WIF** | prefisso 173 · xprv/xpub BIP32 standard |
| **Genesis** | `000008e4586479cd9dc5832b262414a60f152381e371cf7940ffe9f5aa4b80a0` |
| **Seed DNS** | `node1.kotia.cash`, `node2.kotia.cash`, `secondnode.kotia.cash` |

### Privacy: CoinJoin

Il wallet include un motore di mixing **client-side** (ispirato a Dash
PrivateSend / Wasabi): spezza gli importi in output di taglia **random**
(la "denominazione" è il tetto massimo, non l'importo fisso) rompendo
l'euristica "stesso importo = stesso proprietario" usata dall'analisi di
catena.

- Denominazioni: 0.001 · 0.01 · 0.1 · 1 · 10 KOT
- Round di remix configurabili (default 4) — ogni round eredita i round
  degli input
- Tracciamento "rounds" per outpoint, statistiche di mixing, limiti
  mempool-chain con attesa conferme automatica
- Nessun coordinator necessario: la transazione è costruita, firmata e
  trasmessa dal wallet

**Interfaccia Qt**: pagina CoinJoin dedicata (importo, max-tx, round,
progresso per denominazione, stato live, arresto). **RPC**: `coinjoininfo`,
`coinjoinmix <denom> [rounds] [amount]`, `setcoinjoinenabled`.

### Altre funzionalità del wallet

- **Stealth addresses** (prefisso `sm…`): indirizzi one-time non riutilizzabili
- **Staking** integrato (checkbox + peso/expected time nella GUI)
- BIP32 (HD), multisig, coin control, firma/verifica messaggi
- seed DNS + checkpoint per la sincronizzazione

---

## Binari precompilati (cartella `releases/`)

Tutti i binari in `releases/` includono le correzioni dell'audit
agosto 2026 (vedi [CHANGELOG](#changelog-tecnico) sotto).

| File | Sistema | Contenuto |
|---|---|---|
| `releases/windows/kotia-windows-coinjoin.zip` | Windows x64 | `kotia-qt.exe`, `kotiad.exe`, `kotia-cli.exe`, `kotia-tx.exe` |
| `releases/linux/kotia-linux-portable.tar.gz` | Linux x64 | portatile: binari + librerie Qt/boost incluse |
| `releases/macos/kotia-wallet-macos-x86_64.tar.gz` | macOS 10.12+ (x86_64) | `kotia-qt` (statico, dipendenze incluse) |

Avvio veloce (Linux): `tar xf kotia-linux-portable.tar.gz && ./kotia-qt.sh`
<br>macOS: scompatta ed esegui `kotia-qt` (al primo avvio conferma con
tasto destro → Apri).

Per compilare da sorgente vedi [Compilazione](#compilazione).

---

## Compilazione

```bash
./autogen.sh
./configure            # opzioni sotto
make -j$(nproc)
```

Opzioni utili: `--with-gui=qt5` (default) · `--without-gui` (solo nodo) ·
`--disable-tests` · `--with-incompatible-bdb` (BDW 5.3+) · `--disable-bench`

Cross-compile con il sistema `depends/`:

```bash
cd depends && make HOST=x86_64-w64-mingw32 NO_QT=1   # solo daemon Windows
cd .. && CONFIG_SITE=$PWD/depends/x86_64-w64-mingw32/share/config.site \
       ./configure --prefix=/ && make -j$(nproc)
```

Dipendenze (native): g++ ≥5, Qt5, Boost, OpenSSL, libevent, Berkeley DB 5.3
(`libdb5.3++-dev`), miniupnpc (opzionale).

### Test

```bash
make -C src -j$(nproc) test/test_bitcoin
./src/test/test_bitcoin --run_test=coinjoin_tests   # suite CoinJoin
./src/test/test_bitcoin                             # modulo completo
```

---

## RPC CoinJoin — esempi

```bash
# stato e progresso
kotia-cli coinjoininfo

# mix di 10 KOT con transazioni fino a 1 KOT, 4 round
kotia-cli coinjoinmix 1 4 10

# abilita il mixing automatico (persistito nel wallet)
kotia-cli setcoinjoinenabled true
```

---

## Struttura del repository

```
src/coinjoin.{h,cpp}        motore CoinJoin (sessioni, AutoMix, splitter)
src/qt/coinjoinpage.*       pagina CoinJoin della GUI
src/qt/coinjoinworker.*     worker thread per il mixing (UI non-blocking)
src/wallet/, src/           wallet + nodo (Bitcoin Core 0.13/0.14 lineage)
src/test/coinjoin_tests.cpp suite di test CoinJoin (unit + e2e su regtest)
contrib/fix_bloom_testdata.py  rigeneratore vettori di test per chain fork
doc/coinjoin-audit-2026-08-22.md  audit completo del wallet CoinJoin
releases/                   binari precompilati per Windows/Linux/macOS
```

## Changelog tecnico

### v2.4 (2026-08-22) — audit CoinJoin
- **Fix deadlock RPC**: `coinjoinmix` non tiene più `cs_main`/`cs_wallet`
  durante tutto il mixing (il nodo si bloccava per la durata dell'operazione)
- **Fix data race**: status/flag/statistiche ora atomici e protetti da lock
  (thread GUI/RPC/worker)
- **Fix lock-order inversion** in firma/broadcast e selezione monete
- **Fix output dust**: minimo output di mixing alzato sopra la soglia dust
  della chain (minRelayTxFee = 10000 sat/kB)
- **Fix fee**: stima 200 B/input + 10% (elimina i rifiuti "insufficient
  priority" per tx >1KB)
- **Splitter**: `SplitAmountsForMixing()` puro e testato — l'ultimo output
  non può più superare il tetto della denominazione
- Fix underflow `chunkIndex` nel retry mempool-chain; guardie anti
  divisione-per-zero e importi negativi; guardia di re-entrancy in AutoMix
- Il parametro `amount` di `coinjoinmix` ora è onorato; round validati 1–100
- Impostazioni CoinJoin persistite e ricaricate dal wallet DB
- Keypool dei wallet in-memory non apre più BerkeleyDB
- Qt: rimosso timer di stato morto nel worker
- **Nuova suite di test** `coinjoin_tests` (9 casi: unit + e2e completa su
  regtest con vero wallet) e riparazione dell'infrastruttura test della
  chain (PoW scrypt nel fixture, vettori rigenerati per i formati della
  chain) — vedi `doc/coinjoin-audit-2026-08-22.md`

### Storico (dal fork originale)
- v2.13.2.x: fix fake-stake, dust mempool, BDB 6.2.38, OpenSSL 1.0.2u
- v2.13.2.1: base Bitcoin Core 0.13.2 + backport 0.14.x
- v2.3: motore CoinJoin (sessioni, AutoMix multi-round, worker async)
- v2.2: fee-calc fix del mixing

## Licenza

MIT — vedi `COPYING`. Il codice deriva da Bitcoin Core (MIT), con
componenti Novacoin/PPC (MIT) per il Proof-of-Stake.
