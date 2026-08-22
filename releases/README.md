# Prebuilt binaries — Kotia Cash v2.4 (audit 2026-08-22)

All packages include the CoinJoin audit fixes (RPC deadlock, data races,
dust outputs, fee estimation, splitter).

| System | File | SHA256 |
|---|---|---|
| windows | `kotia-windows-coinjoin.zip` | `e1955ab8e272619ade60c80deba2c6007cf0d64518621fd83f4fc9d04034d184` |
| linux | `kotia-linux-portable.tar.gz` | `a6e27da8762a999bf86af2797b02eaac9a217803402dcc3dd8689a889ff1afb8` |
| macos | `kotia-wallet-macos-x86_64.tar.gz` | `3b6e712c7c4c4f3a7d75cf90c292bb1eb358e4df7ada86b18e7871da481edf74` |

## Contents

- **windows/**: `kotia-qt.exe` (GUI), `kotiad.exe` (node), `kotia-cli.exe`,
  `kotia-tx.exe` — x86_64
- **linux/**: portable package — launch with `./kotia-qt.sh` (GUI) or
  `./kotiad.sh` (node); Qt libraries are bundled, no installation required
- **macos/**: `kotia-qt` (static GUI, Qt included), `kotiad`, `kotia-cli`,
  `kotia-tx` — x86_64, macOS 10.12+

## Quick start (Linux)

```bash
tar xf kotia-linux-portable.tar.gz
./kotia-linux-portable/kotia-qt.sh        # GUI wallet
# or headless:
./kotia-linux-portable/kotiad.sh -daemon  # node
./kotia-linux-portable/bin/kotia-cli help
```

## Quick start (Windows)

Unzip the archive and run `kotia-qt.exe` (GUI) or `kotiad.exe` (headless
node) from a command prompt.

## Quick start (macOS)

```bash
tar xf kotia-wallet-macos-x86_64.tar.gz
./kotia-qt          # GUI (right-click -> Open on first launch)
./kotiad -daemon    # headless node
```
