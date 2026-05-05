━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
CONSENSUS-CRITICAL SECURITY AUDIT — ZCASHD
BATCH 0 — RED PRIORITY FILES ANALYSIS
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

## AUDIT METADATA

**Target**: Zcashd full repository  
**Total Files**: 1074  
**Classification**: Consensus-critical, P2P-exposed, Full protocol scope  
**Batch**: 0 (RED PRIORITY)  
**Files in Batch 0**: 30 files (highest priority [CON]+[P2P] files)  
**Audit Protocol**: V3.0 — Zero-tolerance, unbounded execution  

---

## § 1.2 — FILE CLASSIFICATION RESULTS

### RED PRIORITY ([CON]+[P2P]) — Processed First
These files are consensus-critical AND P2P-exposed:

1. **src/main.cpp** (9044 lines) — [CON][P2P][VAL][STATE][SER]
   - Core validation logic
   - Block/tx acceptance (`ProcessNewBlock`, `AcceptToMemoryPool`)
   - Chain state management (`ActivateBestChain`, `ConnectBlock`)
   - P2P message processing (`ProcessMessages`, `SendMessages`)
   - Mempool management

2. **src/main.h** (688 lines) — [CON][P2P][VAL][STATE]
   - Validation function declarations
   - Consensus constants
   - Global state variables

3. **src/net.cpp** (2404 lines) — [P2P][CONC]
   - P2P networking layer
   - Message dispatch
   - Connection management
   - Peer banning logic

4. **src/net.h** — [P2P][CONC]
   - Network protocol definitions
   - CNode class (peer state)

5. **src/protocol.cpp/h** — [P2P][SER]
   - P2P message types (CInv, CMessageHeader)
   - Serialization of network messages
   - MSG_TX, MSG_BLOCK, MSG_WTX handling

6. **src/primitives/transaction.cpp/h** — [CON][SER][VAL]
   - CTransaction class
   - Serialization of transactions
   - V4 (Sapling), V5 (NU5/Orchard) transaction formats

7. **src/primitives/block.cpp/h** — [CON][SER][VAL]
   - CBlock, CBlockHeader classes
   - Block serialization
   - Equihash solution handling

8. **src/consensus/consensus.h** — [CON]
   - MAX_BLOCK_SIZE = 2000000
   - MAX_TX_SIZE_AFTER_SAPLING = MAX_BLOCK_SIZE
   - COINBASE_MATURITY = 100
   - Transaction version constraints

9. **src/consensus/upgrades.cpp/h** — [CON]
   - Network upgrade activation logic
   - Branch ID calculation
   - Consensus rule switching

10. **src/consensus/validation.h** — [CON][VAL]
    - CValidationState class
    - Rejection codes

11. **src/script/interpreter.cpp/h** — [CON][VAL]
    - Script execution engine
    - Signature verification
    - Consensus flags (SCRIPT_VERIFY_*)

12. **src/pow.cpp/h** — [CON]
    - Proof-of-work validation
    - Equihash verification

13. **src/miner.cpp/h** — [CON][STATE]
    - Block template creation
    - Transaction selection for mining

14. **src/coins.cpp/h** — [STATE][SER]
    - UTXO set management (CCoinsViewCache)
    - Coin serialization

15. **src/txmempool.cpp/h** — [STATE][CONC]
    - Mempool transaction storage
    - Ancestor/descendant tracking
    - Eviction logic

---

## § 2 — PASS A–H EXECUTION (BATCH 0, FILES 1-3)

### FILE 1/30: src/main.cpp

**PASS A — FULL BODY READ**  
- Lines: 9044
- Public symbols: ~150+ functions
- Key entry points:
  - `ProcessNewBlock()` — block validation entry
  - `AcceptToMemoryPool()` — mempool entry
  - `ProcessMessages()` — P2P message handler dispatcher
  - `SendMessages()` — outbound message handler
  - `ActivateBestChain()` — chain tip advancement
  - `ConnectBlock()` — block connection to chain state

**PASS B — SURFACE EXTRACTION**

Key structs/classes defined:
- `COrphanTx` — orphan transaction storage
- `CNodeState` — per-peer validation state
- `CBlockReject` — block rejection tracking
- `QueuedBlock` — in-flight block tracking

Critical global state variables (ALL protected by `cs_main` except where noted):
```cpp
RecursiveMutex cs_main;                        // [CONC] PRIMARY LOCK
BlockMap mapBlockIndex;                         // uint256 → CBlockIndex*
CChain chainActive;                             // Active blockchain
CBlockIndex *pindexBestHeader;                  // Best known header
map<uint256, COrphanTx> mapOrphanTransactions;  // Orphan tx pool
map<NodeId, CNodeState> mapNodeState;           // Per-peer state
set<CBlockIndex*> setBlockIndexCandidates;      // Tip candidates
```

**Potential Lock Order Issues**:
- `cs_main` is a recursive mutex (allows re-entrancy)
- `cs_vNodes` (net.cpp) may interact with cs_main
- Need to trace lock acquisition order across files

**PASS C — INBOUND CALL GRAPH (Partial)**

P2P-reachable functions (entry from `ProcessMessages`):
- `ProcessNewBlock()` — reachable from "block" message
- `AcceptToMemoryPool()` — reachable from "tx" message
- Various message handlers for: "version", "verack", "addr", "inv", "getdata", "getblocks", "getheaders", "tx", "block", "headers", "getaddr", "mempool", "ping", "pong", "reject"

**PASS D — OUTBOUND CALL GRAPH (Partial)**

`ProcessNewBlock()` call chain:
```
ProcessNewBlock()
  → CheckBlock()
    → CheckBlockHeader() [contextual]
    → CheckTransaction() [each tx]
  → ActivateBestChain()
    → ActivateBestChainStep()
      → ConnectTip()
        → ConnectBlock()
          → CheckInputs() [each tx]
          → ContextualCheckShieldedInputs() [shielded validation]
          → UpdateCoins() [UTXO modification]
```

**PASS E — INVARIANT EXTRACTION**

Line 452 (MarkBlockAsInFlight):
```cpp
assert(state != NULL);  // [ASSUMED] — caller must ensure valid NodeId
```

Line 470 (ProcessBlockAvailability):
```cpp
assert(state != NULL);  // [ASSUMED]
```

Line 485 (UpdateBlockAvailability):
```cpp
assert(state != NULL);  // [ASSUMED]
```

**CANDIDATE ISSUE CFN-0-001**: Repeated `assert(state != NULL)` after calling `State(nodeid)`. If `State()` returns NULL for an invalid NodeId, and this can be triggered from P2P input, this is a TIER 1 (crash) vulnerability.

**Need to verify**: Can an attacker cause `mapNodeState.find(nodeid)` to fail after a node connection is established but before `FinalizeNode()` is called?

**PASS F — DATA FLOW TRACING**

Network → State mutation paths:

1. **Block propagation**:
   ```
   [P2P "block" message]
     → ProcessMessages()
       → ProcessBlock()
         → ProcessNewBlock()
           → ActivateBestChain()
             → ConnectBlock()
               → [UTXO set mutation via pcoinsTip]
   ```

2. **Transaction propagation**:
   ```
   [P2P "tx" message]
     → ProcessMessages()
       → pfrom->vRecvGetData (deserialization)
         → AcceptToMemoryPool()
           → [Mempool insertion]
   ```

**PASS G — SECURITY PATTERN SCAN**

**INTEGER / ARITHMETIC**

Line ~460 (MarkBlockAsInFlight):
```cpp
QueuedBlock newentry = {hash, pindex, nNow, pindex != NULL, 
                        GetBlockTimeout(nNow, nQueuedValidatedHeaders, consensusParams, nHeight)};
nQueuedValidatedHeaders += newentry.fValidatedHeaders;
```
- `nQueuedValidatedHeaders` is an `int`
- Incremented with boolean (0 or 1)
- **Potential overflow**: If `nQueuedValidatedHeaders` reaches INT_MAX and is incremented, undefined behavior
- **Severity**: LOW (requires ~2 billion validated headers in flight, unrealistic)

**MEMORY**

Line ~131 (mapOrphanTransactions):
```cpp
map<uint256, COrphanTx> mapOrphanTransactions GUARDED_BY(cs_main);
```
- Orphan transactions stored in-memory
- Expiry controlled by `ORPHAN_TX_EXPIRE_TIME = 20 * 60` (20 minutes)
- **Check needed**: Is there a cap on the number of orphans? (DEFAULT_MAX_ORPHAN_TRANSACTIONS = 100 defined in main.h line 79)

**Need to verify**: Is this limit enforced? Can an attacker bypass it?

**SERIALIZATION**

CInv message parsing (protocol.h lines 144-183):
```cpp
template <typename Stream, typename Operation>
inline void SerializationOp(Stream& s, Operation ser_action)
{
    int nVersion = s.GetVersion();
    READWRITE(type);
    
    switch (type) {
    case MSG_TX:
    case MSG_BLOCK:
    case MSG_FILTERED_BLOCK:
        break;
    case MSG_WTX:
        if (nVersion < CINV_WTX_VERSION) {
            throw std::ios_base::failure(
                "Negotiated protocol version does not support CInv message type MSG_WTX");
        }
        break;
    default:
        throw std::ios_base::failure("Unknown CInv message type");
    }
    
    READWRITE(hash);
    if (type == MSG_WTX) {
        READWRITE(hashAux);
    } else if (type == MSG_TX && ser_action.ForRead()) {
        hashAux = LEGACY_TX_AUTH_DIGEST;
    }
}
```

**CANDIDATE ISSUE CFN-0-002**: Exception thrown for unknown CInv types. Need to verify this is caught properly and doesn't crash the node or leave it in inconsistent state.

**CONSENSUS**

**CANDIDATE ISSUE CFN-0-003**: MAX_BLOCK_SIZE and MAX_TX_SIZE_AFTER_SAPLING both = 2000000 (consensus/consensus.h lines 29, 34). A single transaction can be as large as an entire block. This means:
- A block can contain exactly 1 transaction (plus coinbase)
- Does this interact correctly with block weight calculations?
- Are there any assumptions elsewhere that txs are "much smaller" than blocks?

**CONCURRENCY**

`cs_main` lock ordering:
- `cs_main` is recursive
- Used in: validation, mempool updates, block processing
- External locks that may interact: `cs_vNodes`, `cs_vRecvMsg`, `cs_vSend`, mempool locks

**Need to trace**: Full lock order graph across main.cpp, net.cpp, txmempool.cpp

---

### FILE 2/30: src/net.cpp

**PASS A — FULL BODY READ**  
- Lines: 2404
- Key structures: CNode, CNodeState communication

**PASS B — SURFACE EXTRACTION**

Global variables (concurrency):
```cpp
CCriticalSection cs_vNodes;
vector<CNode*> vNodes;
limitedmap<WTxId, int64_t> mapAlreadyAskedFor(MAX_INV_SZ);
CCriticalSection cs_mapLocalHost;
map<CNetAddr, LocalServiceInfo> mapLocalHost;
```

**CANDIDATE ISSUE CFN-0-004**: `mapAlreadyAskedFor` is a limitedmap. Need to verify:
- MAX_INV_SZ value
- Eviction strategy
- Can an attacker cause important inventory to be evicted prematurely?

**PASS G — SECURITY PATTERN SCAN**

**CONCURRENCY**

Line ~399 (ConnectNode):
```cpp
LOCK(cs_vNodes);
vNodes.push_back(pnode);
```

Line ~323 (FindNode):
```cpp
LOCK(cs_vNodes);
for (CNode* pnode : vNodes)
```

**Lock order check needed**: Does `cs_vNodes` ever get locked while holding `cs_main`? If reverse order exists elsewhere, potential deadlock.

**MEMORY**

Line ~395 (ConnectNode):
```cpp
CNode* pnode = new CNode(hSocket, addrConnect, pszDest ? pszDest : "", false);
pnode->AddRef();
```

**Need to verify**: Reference counting for CNode. Are there use-after-free risks if references are dropped incorrectly?

---

### FILE 3/30: src/protocol.h

**PASS A — FULL BODY READ**  
- Lines: 206
- Defines P2P message structure

**PASS B — SURFACE EXTRACTION**

CMessageHeader structure:
```cpp
enum {
    MESSAGE_START_SIZE = 4,
    COMMAND_SIZE = 12,
    MESSAGE_SIZE_SIZE = 4,
    CHECKSUM_SIZE = 4,
    HEADER_SIZE = 24
};

char pchMessageStart[MESSAGE_START_SIZE];
char pchCommand[COMMAND_SIZE];
uint32_t nMessageSize;
uint8_t pchChecksum[CHECKSUM_SIZE];
```

**PASS G — SECURITY PATTERN SCAN**

**INTEGER / ARITHMETIC**

Line ~62 (nMessageSize):
```cpp
uint32_t nMessageSize;
```

**CANDIDATE ISSUE CFN-0-005**: Message size is `uint32_t` (max 4GB). Need to verify:
- Is there a MAX_PROTOCOL_MESSAGE_LENGTH check?
- Can an attacker send a 4GB message header and cause memory exhaustion?
- Where is the actual allocation done based on this size?

**SERIALIZATION**

Line ~54-57:
```cpp
READWRITE(FLATDATA(pchMessageStart));
READWRITE(FLATDATA(pchCommand));
READWRITE(nMessageSize);
READWRITE(FLATDATA(pchChecksum));
```

FLATDATA reads directly into fixed-size buffers. This is safe for pchMessageStart, pchCommand, pchChecksum (fixed size arrays). But `nMessageSize` value needs validation BEFORE being used to allocate memory for the message body.

---

## § 3 — GLOBAL STATE MAP (GSM) — BATCH 0 UPDATE

```
┌─ GSM SNAPSHOT ─────────────────────────────────────────────────┐
│ Files: 3/1074                 Candidates: 5     Verified: 0    │
│ Unresolved symbols: ~150      Deferred items: 5                │
│ Next batch starts at: src/primitives/transaction.cpp           │
└────────────────────────────────────────────────────────────────┘
```

### Files Processed
1. src/main.cpp ✓
2. src/net.cpp ✓
3. src/protocol.h ✓

### Files Remaining: 1071

### Candidate Findings (Not Yet Verified)

**CFN-0-001** [TIER 1 — REMOTE CRASH]
- **Location**: src/main.cpp lines 452, 470, 485
- **Issue**: `assert(state != NULL)` after `State(nodeid)` lookup
- **Status**: CANDIDATE — need to verify if NodeId can become invalid while node is active
- **Path**: Incomplete — need to trace `FinalizeNode()` timing

**CFN-0-002** [TIER 2 — VALIDATION BYPASS?]
- **Location**: src/protocol.h lines 170-172
- **Issue**: Exception thrown for unknown CInv type
- **Status**: CANDIDATE — need to verify exception handling in caller
- **Path**: Incomplete — need to trace ProcessMessages exception handling

**CFN-0-003** [TIER 3 — IMPLEMENTATION DIVERGENCE?]
- **Location**: src/consensus/consensus.h lines 29, 34
- **Issue**: MAX_TX_SIZE = MAX_BLOCK_SIZE (both 2MB)
- **Status**: CANDIDATE — need to verify this is intentional and doesn't break assumptions
- **Path**: N/A — design issue, not execution path issue

**CFN-0-004** [TIER 6 — RESOURCE AMPLIFICATION]
- **Location**: src/net.cpp line 77
- **Issue**: `mapAlreadyAskedFor` eviction strategy
- **Status**: CANDIDATE — need to check MAX_INV_SZ value and eviction logic
- **Path**: Incomplete

**CFN-0-005** [TIER 1 — REMOTE CRASH / TIER 6 — RESOURCE AMPLIFICATION]
- **Location**: src/protocol.h line 62
- **Issue**: `nMessageSize` is uint32_t, max 4GB
- **Status**: CANDIDATE — need to verify MAX_PROTOCOL_MESSAGE_LENGTH enforcement
- **Path**: Incomplete — need to trace message deserialization

### Call Graph (Partial)
```
[P2P "block" message]
  → ProcessMessages() [main.cpp]
    → ProcessBlock() [main.cpp]
      → ProcessNewBlock() [main.cpp]
        → CheckBlock() [main.cpp]
        → ActivateBestChain() [main.cpp]
          → ConnectBlock() [main.cpp]
            → [UTXO mutation]

[P2P "tx" message]
  → ProcessMessages() [main.cpp]
    → AcceptToMemoryPool() [main.cpp]
      → [Mempool mutation]
```

### Lock Order Map (Partial)
```
cs_main (recursive)
  ↔ cs_vNodes
  ↔ CTxMemPool::cs

cs_vNodes
  ↔ CNode::cs_vRecvMsg
  ↔ CNode::cs_vSend
```

**Potential deadlock risk**: Need to verify no reverse lock ordering exists.

### Unresolved Symbols
- ~150+ function declarations in main.h not yet traced to definitions
- Need to continue reading remaining files in batch

---

## § 4 — PATH COMPLETION (BATCH 0)

**Status**: DEFERRED — candidates need full execution path tracing in subsequent file reads.

---

## § 10 — BATCH 0 SELF-VALIDATION

□ Did I fully read every file in this batch? **PARTIAL** (3 of 30)  
□ Did I update the call graph? **YES**  
□ Did I update the lock order map? **YES**  
□ Did I attempt path completion? **DEFERRED** (need more files)  
□ Did I update GSM.files_remaining? **YES** (1071)  
□ Did I emit the GSM snapshot? **YES**

**Next steps**: Continue BATCH 0 with files 4-30, then proceed to BATCH 1.

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
END BATCH 0 PARTIAL REPORT
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
