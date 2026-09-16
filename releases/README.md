# Prebuilt binaries — Kotia Cash v2.4 (audit 2026-08-22)

All packages include the CoinJoin audit fixes (RPC deadlock, data races,
dust outputs, fee estimation, splitter).

| System | File | SHA256 |
|---|---|---|
| windows | `kotia-windows-coinjoin.zip` | `e1955ab8e272619ade60c80deba2c6007cf0d64518621fd83f4fc9d04034d184` |
| linux | `kotia-linux-portable.tar.gz` | `a6e27da8762a999bf86af2797b02eaac9a217803402dcc3dd8689a889ff1afb8` |
| linux | `kotia-wallet-coinjoin-v24.AppImage` | `3d3ebc78e78bb293cf13f187c4840b16e68c8def895f87bc49ea0e647264dbf3` |
| linux | `kotia-qt-linux-v24.tar.gz` | `fb7db7c1bbb4e01ed83d34cdabcc861ca5be320bfc733d10d4749a9eafb48b49` |
| linux | `kotia-daemon-linux-v24.tar.gz` | `9f04a33fc5ed1bd489ff3e2fbcc0fecc456844fa1fd4d0480f9faa1485ed1121` |
| macos | `kotia-wallet-macos-x86_64.tar.gz` | `3b6e712c7c4c4f3a7d75cf90c292bb1eb358e4df7ada86b18e7871da481edf74` |
| macos | `kotia-wallet-macos-arm64.tar.gz` | `5779a81a9ffdbabec311d734cbb5d7546a840549c085cd55d26a45de72f71e1b` |

## Contents

- **windows/**: `kotia-qt.exe` (GUI), `kotiad.exe` (node), `kotia-cli.exe`,
  `kotia-tx.exe` — x86_64
- **linux/**: portable package — launch with `./kotia-qt.sh` (GUI) or
  `./kotiad.sh` (node); Qt libraries are bundled, no installation required
- **linux/**: AppImage (`kotia-wallet-coinjoin-v24.AppImage`) — same build,
  single-file, `chmod +x` and run (forces X11/xcb, bundles known sync nodes)
- **linux/**: `kotia-qt-linux-v24.tar.gz` (GUI + cli + tx) and
  `kotia-daemon-linux-v24.tar.gz` (headless node only), for distro-packaged
  Qt systems
- **macos/**: `kotia-qt` (static GUI, Qt included), `kotiad`, `kotia-cli`,
  `kotia-tx` — x86_64 build for macOS 10.12+, arm64 build (Apple Silicon,
  M1/M2/M3/M4) for macOS 11+

## Quick start (Linux)

```bash
tar xf kotia-linux-portable.tar.gz
./kotia-linux-portable/kotia-qt.sh        # GUI wallet
# or headless:
./kotia-linux-portable/kotiad.sh -daemon  # node
./kotia-linux-portable/bin/kotia-cli help
```

Or, for the AppImage:

```bash
chmod +x kotia-wallet-coinjoin-v24.AppImage
./kotia-wallet-coinjoin-v24.AppImage      # GUI wallet
```

## Quick start (Windows)

Unzip the archive and run `kotia-qt.exe` (GUI) or `kotiad.exe` (headless
node) from a command prompt.

## Quick start (macOS)

```bash
tar xf kotia-wallet-macos-arm64.tar.gz    # Apple Silicon (M1/M2/M3/M4)
# or kotia-wallet-macos-x86_64.tar.gz for Intel Macs
./kotia-qt          # GUI (right-click -> Open on first launch)
./kotiad -daemon    # headless node
```
