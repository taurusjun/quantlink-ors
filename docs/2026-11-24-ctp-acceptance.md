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
ctp_reconciler
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

## 6. L1 runtime wiring (CTP ORS -> FastTrade AccessClient)

Implemented:

```text
C++ SBE generated from quantlink-venue-contract 0.2.0
ContractSink: OrderUpdate, Trade, MarketData, AccountSnapshot,
              PositionSnapshot, VenueSession
AeronFramePublisher: Aeron C++ publication adapter (shared libaeron)
AeronCommandSubscriber: OrderRequest/CancelRequest -> CtpAdapter
ContractRuntime: long-running session/MD/command loop; account/position query thread
main.cpp: --mode runtime
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

## 6.4 Reconnect / epoch / reconciliation

`CtpReconciler` is now wired into `ContractRuntime`:

```text
Disconnected/Failed after READY
  -> runtime.reconnecting epoch=2
  -> VenueSession LOST
  -> queryOrders / queryTrades / queryPositions
  -> CtpReconciler.reconcile(...)
  -> runtime.reconciliation complete epoch=2
  -> VenueSession READY

During reconciliation:
  new OrderRequest / CancelRequest are not sent to CTP
  reconciler.canSubmit(orderRef) gates reused orderRefs
```

Real disconnect test on 190 (2026-11-25):

```text
temporary iptables INPUT/OUTPUT drop to 182.254.243.31 for 120s
[ctp] front disconnected reason=8193
runtime reconnecting epoch=2
[ctp] state=AUTHENTICATING -> ... -> READY
runtime reconciliation complete epoch=2
post-reconnect order reached ACK (price did not fill)
```

Strong reconciliation scenario (2026-11-25 14:38-14:56):

```text
OPEN order 400000001 @730.0 -> ACK
network drop 120s -> epoch=2 -> reconciliation complete
re-submit same orderRef -> REJECTED (orderRef already in use)
cancel original -> CANCELED
```

Note: duplicate protection was observed at the CtpAdapter orderRef map. The
runtime `allowsNewOrder` policy and `setEpoch`/cancel-rejection paths now have
unit tests; a live duplicate that bypasses the adapter map is still not injected.

## 7. Known gaps

```text
1. Strong old-epoch report isolation (stale CTP callbacks) still needs a dedicated
   test; duplicate orderRef is protected by the adapter map.
2. GBK -> UTF-8 conversion and unit test are done; console rendering depends on
   the JVM output encoding.
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
