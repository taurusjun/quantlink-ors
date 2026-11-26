# CTP ORS 0.2.0 Conformance Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 按公司 China ORS 语义修正 CTP 的 TIF 映射、`OnRspOrderInsert(ErrorID=0)` 和昨仓计算。

**Architecture:** 只改 `ctp/src` 与 `ctp/tests`，不动 contract/L2。先写失败测试，190 构建/ctest，再改实现。

**Tech Stack:** C++17、CTP SDK、CMake/ctest。

**Spec:** `docs/v2/2026-11-25-ctp-ors-changes.md`

## Global Constraints

```text
只在 190 构建/测试
SHFE
FAK -> IOC + AV + MinVolume=1
IOC -> IOC + CV + MinVolume=qty
FOK -> IOC + CV + MinVolume=qty
DAY -> GFD + AV + MinVolume=1
昨仓 = max(0, Position - TodayPosition)
```

## Review Focus

- FAK 是 tb_origin CROSS 的路径，不能映射成 CV。
- OnRspOrderInsert 即使 ErrorID=0 也必须进入 REJECTED。
- YdPosition 过期时不能高估昨仓。
- 不改 L2 的 AUTO/预扣/整笔 fallback。

---

### Task 1: TIF / FAK 映射

**Files:**
- Modify: `ctp/src/CtpAdapter.cpp`
- Modify: `ctp/tests/CtpAdapterTest.cpp`

- [ ] **Step 1: Update tests first**

In `checkTimeInForceMapping`:

```cpp
check(fixture.sender.lastInsert.VolumeCondition == THOST_FTDC_VC_CV,
      "IOC requires the full volume");
check(fixture.sender.lastInsert.MinVolume == 2, "IOC sets minimum volume");

OrderRequest fak = baseRequest(11);
fak.timeInForce = TimeInForce::Fak;
check(fixture.adapter.onOrderRequest(fak), "a FAK order is accepted");
check(fixture.sender.lastInsert.VolumeCondition == THOST_FTDC_VC_AV,
      "FAK accepts any volume");
check(fixture.sender.lastInsert.MinVolume == 1, "FAK sets minimum volume 1");
```

In `checkInsertAccepted`, add:

```cpp
check(fixture.sender.lastInsert.MinVolume == 1, "DAY sets minimum volume 1");
```

- [ ] **Step 2: Run on 190, expect fail**

```bash
rsync -a ctp/ root@101.43.54.190:/opt/quantlink-ors/ctp/
ssh root@101.43.54.190 'cd /opt/quantlink-ors && cmake --build build -j && ctest --test-dir build --output-on-failure'
```

Expected: `ctp_adapter` FAIL on IOC/FAK expectations.

- [ ] **Step 3: Implement mapping**

`CtpAdapter::fillInsert`:

```cpp
case TimeInForce::Ioc:
  TimeCondition = IOC;
  VolumeCondition = CV;
  MinVolume = request.qty;
  break;
case TimeInForce::Fak:
  TimeCondition = IOC;
  VolumeCondition = AV;
  MinVolume = 1;
  break;
case TimeInForce::Fok:
  TimeCondition = IOC;
  VolumeCondition = CV;
  MinVolume = request.qty;
  break;
case TimeInForce::Day:
  TimeCondition = GFD;
  VolumeCondition = AV;
  MinVolume = 1;
  break;
```

- [ ] **Step 4: Run on 190, expect pass**

Same build/ctest command.

- [ ] **Step 5: Commit**

```bash
git add ctp/src/CtpAdapter.cpp ctp/tests/CtpAdapterTest.cpp
git commit -m "fix(ctp): align time-in-force mapping with company ORS"
```

---

### Task 2: OnRspOrderInsert ErrorID=0

**Files:**
- Modify: `ctp/src/CtpAdapter.cpp`
- Modify: `ctp/tests/CtpAdapterTest.cpp`

- [ ] **Step 1: Add failing test**

Add `checkInsertRejectedWithZeroError` mirroring `checkInsertRejectedByCtp` but with `rspError(0, "")`; expect `OrderStatus::Rejected`, `errorCode == 0`, `remainingQty == 0`.

Add call in `main()`.

- [ ] **Step 2: Run on 190, expect fail**

Same build/ctest.

- [ ] **Step 3: Implement**

Remove the `ErrorID == 0` early return in `onInsertRejected`; for a known NEWORDER always publish REJECTED and `markRejected`.

- [ ] **Step 4: Run on 190, expect pass**

- [ ] **Step 5: Commit**

```bash
git add ctp/src/CtpAdapter.cpp ctp/tests/CtpAdapterTest.cpp
git commit -m "fix(ctp): reject insert response even when error id is zero"
```

---

### Task 3: 昨仓计算

**Files:**
- Modify: `ctp/src/CtpQueryService.cpp`
- Modify: `ctp/tests/CtpQueryServiceTest.cpp`

- [ ] **Step 1: Add failing test**

Add `checkStaleYdPositionIsNotUsed`:

```cpp
auto row = positionRow(THOST_FTDC_PD_Long, 3, 1, 3 * 3400.0 * 10, 3390.0);
row.YdPosition = 9;  // stale
auto snapshot = mapPositionSnapshot(row, batch(), 10);
check(snapshot.yesterdayLong == 2, "yesterday uses Position - TodayPosition");
```

Add call in `main()`.

- [ ] **Step 2: Run on 190, expect fail**

- [ ] **Step 3: Implement**

```cpp
const int yesterday = std::max(0, position - today);
```

- [ ] **Step 4: Run on 190, expect pass**

- [ ] **Step 5: Commit**

```bash
git add ctp/src/CtpQueryService.cpp ctp/tests/CtpQueryServiceTest.cpp
git commit -m "fix(ctp): derive yesterday position from total minus today"
```
