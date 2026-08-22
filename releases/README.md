# Binari precompilati — Kotia Cash v2.4 (audit 2026-08-22)

Tutti i pacchetti includono le correzioni dell'audit CoinJoin (deadlock RPC, data race, dust, fee, splitter).

| Sistema | File | SHA256 |
|---|---|---|
| windows | `kotia-windows-coinjoin.zip` | `e1955ab8e272619ade60c80deba2c6007cf0d64518621fd83f4fc9d04034d184` |
| linux | `kotia-linux-portable.tar.gz` | `a6e27da8762a999bf86af2797b02eaac9a217803402dcc3dd8689a889ff1afb8` |
| macos | `kotia-wallet-macos-x86_64.tar.gz` | `3b6e712c7c4c4f3a7d75cf90c292bb1eb358e4df7ada86b18e7871da481edf74` |

## Contenuto
- **windows/**: `kotia-qt.exe` (GUI), `kotiad.exe` (nodo), `kotia-cli.exe`, `kotia-tx.exe` — x86_64
- **linux/**: pacchetto portatile — avvia con `./kotia-qt.sh` (GUI) o `./kotiad.sh` (nodo); librerie Qt incluse, nessuna installazione richiesta
- **macos/**: `kotia-qt` (GUI statica, Qt incluso), `kotiad`, `kotia-cli`, `kotia-tx` — x86_64, macOS 10.12+
