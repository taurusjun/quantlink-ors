# 2026-11-24 CTP/SimNow Acceptance

- 状态：PARTIAL（L1 E2E PASS；重连基本链路 PASS，强对账/旧回报未完整验收）
- 环境：190 (`root@101.43.54.190`), SimNow `ors-simnow.cfg`
- 代码：`52f4c9f feat(ctp): publish L0 contract 0.2.0` + `dae6702 feat(ctp): reconcile after reconnect` + 当前 E2E wiring
- CTP SDK：`/root/quantlink-trading/ors/third_party/ctp/linux`
- Aeron headers：`/opt/quantlink-fin/aeron-src`
- Aeron runtime：`/opt/quantlink-fin/aeron-runtime`

## 1. Build / ctest

```bash
SBE_JAR=/root/sbe-all-1.35.6.jar \
CONTRACT_HOME=/opt/quantlink-venue-contract \
./tools/gen_contract_cpp.sh
cmake -S . -B build -DCTP_HOME=/root/quantlink-trading/ors/third_party/ctp/linux \
      -DAERON_HOME=/opt/quantlink-fin/aeron-src \
      -DAERON_RUNTIME_HOME=/opt/quantlink-fin/aeron-runtime \
      -DCONTRACT_GENERATED_DIR=/root/quantlink-ors/build/generated/contract
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Result:

```text
100% tests passed, 0 tests failed out of 14
```

Covered:

```text
ctp_config / ctp_md / ctp_query / ctp_order_ref_map / ctp_adapter
ctp_contract_sink
ctp link/mode smoke
```

## 2. SimNow login

```text
state=CONNECTING -> AUTHENTICATING -> LOGGING_IN
     -> SETTLEMENT_CONFIRMING -> QUERYING_INSTRUMENTS -> READY
READY instruments=16706
```

The instrument query can take more than the old 30s; smoke uses 120s.

## 3. Account / position / order / trade query

```text
account snapshot=20261125-ACC-1 tradingDay=20261125 account=142266
  balance=2.60277e+07 available=2.16966e+07 margin=3.57954e+06
positions rows=2
  ag2706 SHFE short=88 yesterdayShort=88
  au2612 SHFE long=9 todayLong=2 yesterdayLong=7
orders rows=3
  ref=1 status=5 canceled
  ref=1 status=0 filled
  ref=1 status=0 filled
trades rows=2
  au2612 price=778.5 qty=1
  au2612 price=777.56 qty=1
```

Note: the three order rows all show `ref=1` because each smoke process starts a new CTP
flow and the smoke-only orderRef space is reused. Production reconciliation must key by
the CTP `(FrontID, SessionID, OrderRef)` triple and the contract `orderRef`.

## 4. Market data

```text
md READY ticks=1 instrument=au2612 update=00:04:06.500
last=766.48 volume=103703 levels=20
bids 766.42@2 + 19 zero levels
asks 766.48@1 + 19 zero levels
```

## 5. Live order / cancel / fill

Earlier live runs on the same CTP code:

```text
cancel path: NEW -> ACK -> CANCELED, 6 OrderUpdates
fill path:   FILLED, 5 OrderUpdates + 1 Trade
trade:       ref=1 id=80797 price=777.56 qty=1
```

This verifies:

```text
ReqOrderInsert -> OnRtnOrder
ReqOrderAction -> OnRtnOrder(CANCELED)
OnRtnTrade -> Trade
```

## 5.1 Live lifecycle re-run after replay cleanup

After removing L0 trade deduplication and the empty-`tradeId` drop path, both
order lifecycles were re-run on the 2026-11-25 night session:

```text
cancel path:
  NEW -> ACK (sysId=170724) -> CANCELED
  no Trade, no padded IDs

fill path:
  NEW -> Trade id=103244 price=741.48 qty=1
  -> FILLED (sysId=171097)
  no padded IDs
```

A follow-up query confirmed the fills and the current position:

```text
position au2612 long=18 todayLong=2 short=0
trades: id=102465 price=740.0, id=103244 price=741.48
```

This also confirmed that CTP's fixed-width `TradeID` / `OrderSysID` fields are
trimmed before entering the contract; the earlier live run exposed leading
spaces (`"      102465"`).

## 5.2 Live cancel reject

`ors_ctp_smoke --mode cancel-reject-smoke --live` was added to exercise a real
CTP cancel reject instead of a mocked callback:

```text
NEW -> ACK (sysId=189761)
raw ReqOrderAction FrontID=0 SessionID=0 -> rc=0
ACK cancel=REJECTED error=25 reason=CTP:撤单找不到相应报单
cleanup cancel -> CANCELED
```

CTP delivered both `OnRspOrderAction` and `OnErrRtnOrderAction` for the one
request; L0 now publishes only one `cancelState=REJECTED` update per cancel
attempt. The order stayed ACK (not CANCELED) until the cleanup cancel.

Connection-generation guard: `CtpAdapter` now compares `FrontID` / `SessionID`
from the login response against incoming order/cancel reports and drops reports
from an old front. `OnRtnTrade` uses the order's bound identity. The deterministic
case is covered by `CtpAdapterTest::checkOldSessionReportsDropped`; a real old
front callback is not naturally reproducible on SimNow, so it remains a
fault-injection test rather than a live venue replay.

Current 190 ctest: 16/16 PASS (`ctp_smoke_lifecycle` added;
`ctp_reconciler` removed with the ORS order-state cleanup).

## 5.3 Single-login live lifecycle

The first live smoke implementation logged in once per mode, so rapid
cancel/fill/query runs hit SimNow session pressure and stalled in
`QUERYING_INSTRUMENTS`. The smoke now has one `lifecycle-smoke` mode:

```text
one login -> cancel-reject case -> fill case -> account/position query
```

Live result on the production SimNow config (2026-11-26):

```text
READY instruments=16860
CANCEL-CASE rejected=true cleanup=CANCELED
FILL-CASE status=FILLED trades=1
trade id=267812 price=767.72 qty=1
accountSnapshot=1 positionSnapshot=1
```

TDD test (`ctp_smoke_lifecycle`) was written first and observed failing:

```text
RED:   5 assertions failed (login count/order/stop-after-failure)
GREEN: login exactly once -> cancel -> fill -> query
```

Terminal live state follows company ORS `RemoveFromMap` semantics: after a
Canceled/Rejected/Filled terminal update is published, the live `orders_` and
OrderRef mapping are erased. There is no tombstone and no pending buffer; unknown
reports are dropped. L2 guarantees unique orderRefs.

## 6. L1 runtime wiring (CTP ORS -> FastTrade AccessClient)

Implemented:

```text
C++ SBE generated from quantlink-venue-contract 0.2.0
ContractSink: OrderUpdate, Trade, MarketData, AccountSnapshot,
              PositionSnapshot, VenueSession
AeronFramePublisher: Aeron C++ publication adapter (shared libaeron)
AeronCommandSubscriber: OrderRequest/CancelRequest -> CtpAdapter
ContractRuntime: long-running session/MD/command loop; account/position query thread
main.cpp: CTP ORS runtime only; smoke tools are in ctp::ors_ctp_smoke
```

Test stream layout (matching FastTrade `AccessConfig.singleChannel` base 9000):

```text
MD 9001, order 9002, cancel 9003, execution 9004,
position 9005, account 9006, session 9007
```

Observed on 190 with one runtime process and FastTrade AccessSmoke:

```text
runtime READY venue=SIMNOW account=142266 ors=ors-ctp-e2e streams=9004,9001
AccessClient: session=1/READY, MD au2612 exchange=SHFE ask=765.98
MD decoded with 20 levels; order submitted on Aeron 9002
CTP OrderUpdate: NEW -> (exchange rejection) REJECTED
```

The early-night rejection was not an L1 wiring failure: the test ran after the
SHFE night session closed. On 2026-11-25 11:02 CST, during the day session, the
same path reached `NEW -> ACK -> FILLED` and emitted a Trade.

The first live Trade frame exposed two C++/contract integration bugs, both fixed:

```text
trade.requestId was empty -> fill it from the order state
snapshotTimestamp/ingestTimestamp were milliseconds -> emit epoch nanoseconds
```

The final live E2E was re-run on 2026-11-25 during the afternoon session and
passed end-to-end:

```text
runtime sourceSession=1795584979085096323 epoch=1
MD ask=759.30
orderRef=323450831
NEW -> ACK -> FILLED
Trade tradeId=266036 qty=1 price=759.30
FASTTRADE_MVP_OK (fresh Account/Position passed)
```

Also completed after that run:

```text
CTP ErrorMsg/StatusMsg GB18030 -> UTF-8 conversion + unit test
sourceSessionId generated per runtime start; sourcePublicationEpoch configurable
AccessSmoke takes sourceSessionId/epoch from the live VenueSession
ctest: 14/14 PASS
```
Runtime account/position queries are published by a separate thread, so they do
not block session/MD.

An independent L1 snapshot client also verified the runtime query thread:

```text
SNAPSHOT ready=true account=true positions=2
```

So the L1 wiring is **verified through command submission, execution publication,
account/position snapshots, and one live fill** (Trade + fresh snapshots).

## 6.4 Reconnect / epoch

当前 runtime behavior after the order-ownership cleanup:

```text
Disconnected/Failed after READY
  -> runtime.reconnecting epoch=2
  -> VenueSession LOST
  -> re-login / settlement confirm
  -> runtime.reconnected epoch=2
  -> VenueSession READY

Periodically while READY:
  queryAccount / queryPositions
  -> AccountSnapshot / PositionSnapshot
```

The runtime no longer calls `queryOrders` / `queryTrades`, no longer has
`CtpReconciler`, and does not build order state. Account/position snapshots are
still L0 facts published for L2/AccountSnapshotService; they are not the account
truth and do not gate READY. Order records and order reconciliation belong to L2
`OrderRegistry` / `TradingChannel` and L3 FactPublisher / L4 LocalAgent / L5 ReadModel.
Query failures do not block the CTP session READY transition.

Historical note: the 2026-11-25 iptables reconnect runs below were executed
against the older runtime that still had `CtpReconciler` and order/trade queries.
They prove the CTP disconnect/re-login/epoch path, but the order-reconciler
portion is no longer part of the current design.

```text
temporary iptables INPUT/OUTPUT drop to 182.254.243.31 for 120s
[ctp] front disconnected reason=8193
runtime reconnecting epoch=2
[ctp] state=AUTHENTICATING -> ... -> READY
```

```text
cycle 1: block 120s -> epoch=2
cycle 2: block 120s -> epoch=3
cycle 3: block 120s -> epoch=4
```

Each cycle returned READY; no iptables rules or runtime process remained.

## 7.1 Residual risk assessment (accepted temporarily)

The following gap is deliberately **not fixed** for the current SimNow/test
phase. It must be reassessed before any live-money deployment.

### R1. Old CTP callbacks can be stamped with the new epoch

Current runtime behavior:

```text
ContractRuntime::onOrderUpdate/onTrade
  -> always writes currentEpoch into sourcePublicationEpoch
```

Partial mitigation: `CtpAdapter` compares the login response's `FrontID` /
`SessionID` with `OnRtnOrder`, cancel reports and the bound order of
`OnRtnTrade`; reports from a different old front are dropped before the runtime
stamps them. Residual case: a new login that reuses the same
`FrontID` / `SessionID` cannot be distinguished by this check.

If the CTP SDK delivers an old-session order/trade callback after reconnect, it
will not be recognised as stale. A stale `FILLED` or `Trade` could therefore be
counted again by L2.

Risk judgment:

```text
Trigger probability: low/unknown
  No evidence yet that the CTP SDK replays old-front callbacks after reconnect;
  this must be confirmed against the SDK or a packet/callback capture.

Impact if triggered: medium-high
  Wrong order state or duplicate fill accounting at L2. Duplicate order
  submission is still blocked by CtpAdapter's orderRef map.

Overall: medium live risk, not acceptable as a long-term production posture.
Current SimNow/test phase: accepted temporarily.
```

Required before live money:

```text
stamp callbacks with a CTP connection generation or verify FrontID/SessionID,
and make runtime preserve/ignore the event epoch instead of overwriting it.
```

### R2. Runtime orderRef gate

Removed. ORS no longer keeps `CtpReconciler` or `canSubmit`; orderRef ownership
and duplicate protection belong to L2/LocalAgent/ReadModel. The CTP adapter still rejects
a duplicate orderRef within its own live routing map.

## 7. Known gaps

```text
1. Strong old-epoch report isolation (stale CTP callbacks) still needs a dedicated
   test; duplicate orderRef is protected by the adapter map.
2. GBK -> UTF-8 conversion and unit test are done; console rendering depends on
   the JVM output encoding.
3. ORS order/trade query masks and runtime reconciliation were removed; L2/LocalAgent/ReadModel
   order reconciliation still needs its own acceptance gate.
```

Therefore this acceptance is **PARTIAL**, not a production L1 end-to-end sign-off.

## 8. Next acceptance gate

```text
reconnect gate:
  start one long-running ORS process
  -> force disconnect / reconnect
  -> epoch increments, old reports discarded
  -> open orders reconciled, no duplicate orderRef submission
```

L1 order/fill/account/position gate: PASS on 2026-11-25 afternoon.
