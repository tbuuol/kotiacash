# Graph Report - /tmp/coinjoin-corpus  (2026-05-20)

## Corpus Check
- Corpus is ~9,397 words - fits in a single context window. You may not need a graph.

## Summary
- 80 nodes · 113 edges · 8 communities detected
- Extraction: 96% EXTRACTED · 4% INFERRED · 0% AMBIGUOUS · INFERRED: 4 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- [[_COMMUNITY_CoinJoin Page UI|CoinJoin Page UI]]
- [[_COMMUNITY_AutoMix Transaction Creation|AutoMix Transaction Creation]]
- [[_COMMUNITY_Wallet Accounting|Wallet Accounting]]
- [[_COMMUNITY_Denomination Selection|Denomination Selection]]
- [[_COMMUNITY_Session Lifecycle|Session Lifecycle]]
- [[_COMMUNITY_Session Config & Data|Session Config & Data]]
- [[_COMMUNITY_Progress & Coin Selection|Progress & Coin Selection]]
- [[_COMMUNITY_Wallet Binding|Wallet Binding]]

## God Nodes (most connected - your core abstractions)
1. `updateProgressBars()` - 6 edges
2. `CreateCoinJoinTransaction()` - 6 edges
3. `setBalance()` - 5 edges
4. `updateStakingInfo()` - 5 edges
5. `GetStandardDenominations()` - 5 edges
6. `Init()` - 4 edges
7. `CAccountingEntry()` - 4 edges
8. `CoinJoinPage()` - 4 edges
9. `updateStatus()` - 4 edges
10. `GetOutpointRounds()` - 4 edges

## Surprising Connections (you probably didn't know these)
- `updateProgressBars()` --calls--> `GetStandardDenominations()`  [INFERRED]
  coinjoinpage.cpp → coinjoin.cpp
- `CreateCoinJoinTransaction()` --calls--> `CTransaction()`  [INFERRED]
  coinjoin.cpp → wallet.h
- `CreateCoinJoinTransaction()` --calls--> `CWalletTx()`  [INFERRED]
  coinjoin.cpp → wallet.h
- `GetSession()` --calls--> `CoinJoinSession()`  [INFERRED]
  coinjoin.cpp → coinjoin.h

## Communities

### Community 1 - "CoinJoin Page UI"
Cohesion: 0.19
Nodes (11): CoinJoinPage(), on_denominationComboBox_currentIndexChanged(), on_enableStakingCheckbox_toggled(), on_mixMaxButton_clicked(), on_startMixButton_clicked(), setBalance(), setWalletModel(), updateDisplayUnit() (+3 more)

### Community 3 - "AutoMix Transaction Creation"
Cohesion: 0.29
Nodes (8): AutoMix(), CreateCoinJoinTransaction(), MarkMixed(), SignAndBroadcast(), CTransaction(), CWalletTx(), Init(), txIn()

### Community 4 - "Wallet Accounting"
Cohesion: 0.4
Nodes (5): CAccount(), CAccountingEntry(), ReadOrderPos(), SetNull(), WriteOrderPos()

### Community 5 - "Denomination Selection"
Cohesion: 0.4
Nodes (5): CreateSession(), GetDenominations(), GetStandardDenominations(), IsValidDenomination(), SuggestDenomination()

### Community 6 - "Session Lifecycle"
Cohesion: 0.5
Nodes (4): CleanupExpiredSessions(), GetActiveSessions(), Init(), IsExpired()

### Community 7 - "Session Config & Data"
Cohesion: 0.5
Nodes (2): CoinJoinSession(), GetSession()

### Community 8 - "Progress & Coin Selection"
Cohesion: 0.67
Nodes (3): GetMixingProgress(), GetOutpointRounds(), SelectCoinsForDenomination()

### Community 9 - "Wallet Binding"
Cohesion: 1.0
Nodes (2): BindWallet(), MarkDirty()

## Knowledge Gaps
- **Thin community `Session Config & Data`** (4 nodes): `CoinJoinSession()`, `GetSession()`, `SetConfig()`, `coinjoin.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Wallet Binding`** (2 nodes): `BindWallet()`, `MarkDirty()`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `CreateCoinJoinTransaction()` connect `AutoMix Transaction Creation` to `Progress & Coin Selection`, `CoinJoin Manager Core`?**
  _High betweenness centrality (0.484) - this node is a cross-community bridge._
- **Why does `GetStandardDenominations()` connect `Denomination Selection` to `CoinJoin Page UI`, `CoinJoin Manager Core`?**
  _High betweenness centrality (0.357) - this node is a cross-community bridge._
- **Why does `updateProgressBars()` connect `CoinJoin Page UI` to `Denomination Selection`?**
  _High betweenness centrality (0.343) - this node is a cross-community bridge._
- **Are the 2 inferred relationships involving `CreateCoinJoinTransaction()` (e.g. with `CWalletTx()` and `CTransaction()`) actually correct?**
  _`CreateCoinJoinTransaction()` has 2 INFERRED edges - model-reasoned connections that need verification._
- **Should `Wallet Balance Methods` be split into smaller, more focused modules?**
  _Cohesion score 0.1 - nodes in this community are weakly interconnected._