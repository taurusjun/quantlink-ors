# 2026-11-24 CTP/SimNow Acceptance

- 状态：PARTIAL
- 环境：190 (`root@101.43.54.190`), SimNow `ors-simnow.cfg`
- 代码：`52f4c9f feat(ctp): publish L0 contract 0.2.0` + `dae6702 feat(ctp): reconcile after reconnect`
- CTP SDK：`/root/quantlink-trading/ors/third_party/ctp/linux`
- Aeron：`/opt/quantlink-fin/aeron-src`

## 1. Build / ctest

```bash
SBE_JAR=/root/sbe-all-1.35.6.jar \
CONTRACT_HOME=/opt/quantlink-venue-contract \
./tools/gen_contract_cpp.sh
cmake -S . -B build -DCTP_HOME=/root/quantlink-trading/ors/third_party/ctp/linux \
      -DCONTRACT_GENERATED_DIR=/root/quantlink-ors/build/generated/contract
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Result:

```text
100% tests passed, 0 tests failed out of 12
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

## 6. Contract 0.2.0 output

Implemented and unit-tested:

```text
C++ SBE generated from quantlink-venue-contract 0.2.0
ContractSink: OrderUpdate, Trade, MarketData, AccountSnapshot,
              PositionSnapshot, VenueSession
AeronFramePublisher: Aeron C++ publication adapter
```

Not yet wired / not yet accepted end-to-end:

```text
main.cpp does not yet publish VenueSession/MarketData/Execution via ContractSink
FastTrade AccessClient has not subscribed to a live ORS ContractSink yet
CtpReconciler is unit-tested but not wired into the reconnect loop
manual disconnect/reconnect smoke was not run
```

Therefore this acceptance is **PARTIAL**, not a production L1 end-to-end sign-off.

## 7. Known gaps

```text
1. ContractSink/AeronPublisher are built but not wired into the ORS runtime.
2. CtpReconciler is not called after a real reconnect.
3. Smoke orderRef is process-local; production orderRef/epoch must be durable.
4. VenueSession publication and lastSeqByStream high-water are not live yet.
```

## 8. Next acceptance gate

```text
start one long-running ORS process
  -> publish VenueSession READY
  -> publish MarketData 20 levels
  -> publish Account/Position snapshots
  -> FastTrade AccessClient subscribes and sees all streams
  -> one live order/cancel/fill
  -> disconnect/reconnect, epoch changes, old reports discarded
```
