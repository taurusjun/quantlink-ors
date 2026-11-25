# 2026-11-24 CTP/SimNow Acceptance

- 状态：PARTIAL
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

The final Trade decode + fresh Account/Position assertion has to be re-run after
the 11:30-13:30 lunch break; 11:33 orders were rejected by the closed exchange.

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
and account/position snapshots**, but **live fill/cancel still need a trading-hours
run**.

## 7. Known gaps

```text
1. Final live Trade decode + fresh Account/Position assertion after the timestamp
   fix; the 11:33 CST run hit the lunch break.
2. CtpReconciler is unit-tested but not wired into the reconnect loop.
3. sourceSessionId is generated per start and epoch is configurable, but automatic
   epoch increment on reconnect still belongs to the C reconnect work.
4. Manual disconnect/reconnect and stale-report discard smoke was not run.
5. GBK -> UTF-8 conversion and its unit test are done; a live CTP error reason
   assertion on the L1 wire is still pending.
```

Therefore this acceptance is **PARTIAL**, not a production L1 end-to-end sign-off.

## 8. Next acceptance gate

```text
rerun during exchange trading hours:
  start one long-running ORS process
  -> FastTrade AccessClient sees VenueSession READY + MarketData
  -> one live order/fill
  -> FastTrade sees OrderUpdate/Trade and fresh Account/Position
  -> optional cancel path
  -> disconnect/reconnect, epoch changes, old reports discarded
```
