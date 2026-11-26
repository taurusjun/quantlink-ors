# L0 ORS 生产化实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 按 `docs/v3/l0-ors-design.md` 补齐 L0 ORS 的生产化缺口，不改变 L0/L1/L2 边界。

**Architecture:** L0 只保留 CTP 会话、live 路由、contract/Aeron 发布和 venue 快照；不查询订单、不持有订单状态、不做 RMS。每个任务先写失败测试，再写最小实现，最后在 190 构建/ctest。

**Tech Stack:** C++20, CMake, CTP TraderApi/MdApi, Aeron IPC, SBE contract 0.2.0, 190 Rocky Linux.

**Spec:** `/Users/wangjun/PG/quantlink-new/quantlink-ors/docs/v3/l0-ors-design.md`

## Global Constraints

```text
C++20，构建/测试在 190
contract 0.2.0，SBE 字段固定宽度
ORS 不做订单查询/订单状态权威/RMS
orderRef 由 L2 分配；终态删除 live 映射
account/position 查询保留，发布快照，不 gate READY
所有改动先 RED 测试，再 GREEN，再 REFACTOR
不在生产二进制加入 smoke 逻辑
```

## Review Focus

1. UTF-8 多字节字符在 contract 64/32/16 字节边界不能被截断成非法序列。
2. Aeron back-pressure/not-connected 时执行帧不能静默丢失。
3. MD 断线后必须能重新登录；旧 tick 不能当新行情重复发布。
4. CTP `Failed` 必须是可观察终态，不能永久僵尸。
5. 入站 `accountId/venueId/environment` 不匹配必须拒绝。
6. account/position 查询不能形成背靠背突发。

---

### Task 0: 基础库（配置、日志、指标）

**Files:**
- Create: `third_party/inih/ini.c`
- Create: `third_party/inih/ini.h`
- Create: `ctp/include/ors/base/ConfigFile.h`
- Create: `ctp/include/ors/base/Log.h`
- Create: `ctp/include/ors/base/Metrics.h`
- Create: `ctp/include/ors/base/StringList.h`
- Create: `ctp/src/base/ConfigFile.cpp`
- Create: `ctp/src/base/Log.cpp`
- Create: `ctp/src/base/Metrics.cpp`
- Create: `ctp/src/base/StringList.cpp`
- Create: `ctp/tests/ConfigFileTest.cpp`
- Create: `ctp/tests/LogTest.cpp`
- Create: `ctp/tests/MetricsTest.cpp`
- Create: `ctp/tests/StringListTest.cpp`
- Modify: `ctp/CMakeLists.txt`
- Modify: `ctp/src/ContractRuntime.cpp`
- Modify: `ctp/src/CtpTraderSession.cpp`
- Modify: `ctp/src/CtpMdSession.cpp`
- Move: current `ConfigFile`/`StringList` out of `ors_ctp_config`

**Interfaces:**
- `ors::base::ConfigFile` with `load/get/contains/require`, backed by vendored inih.
- `ors::base::logEvent(spdlog::logger&, level, std::string_view, std::initializer_list<LogField>);`
- `ors::base::Metrics` atomic counters + `logMetrics`.
- `std::vector<std::string> ors::base::splitList(std::string_view);`

- [ ] **Step 1: Write failing tests**
  - ConfigFile：整行 `#`/`;` 注释；值中的 `#` 保留；section 拒绝；缺 key 报错不含值；坏行带行号。
  - StringList：逗号/空格拆分。
  - Metrics：计数器递增、汇总输出字段。
  - Log：输出合法 JSONL；字符串引号/换行转义；数字保持 number；凭据字段脱敏；UTF-8 安全截断。

- [ ] **Step 2: Run RED**
  - `ctest -R "ctp_config_file|ctp_log|ctp_metrics|ctp_string_list"` 失败。

- [ ] **Step 3: Implement**
  - 新建 `ors_base` target；`ors_ctp_config` 链接 `ors_base`。
  - vendor inih，编译定义：
    ```text
    INI_ALLOW_INLINE_COMMENTS=0
    INI_ALLOW_MULTILINE=0
    INI_ALLOW_NO_VALUE=0
    INI_MAX_LINE=4096
    ```
  - ConfigFile 用 `ini_parse` 回调建 map；section 非空返回 fail；整数/宽度仍由上层校验。
  - 引入 pin 版本 spdlog；async logger，queue 8192，`overrun_oldest`，rotating 100MB x 5。
  - `Log.h` 封装 spdlog，业务代码不直接用 spdlog 宏。
  - `Metrics` 使用 relaxed atomic；60 秒汇总。
  - 按 `docs/v3/logging.md` 的关键位置替换 `std::cerr`。

- [ ] **Step 4: Verify GREEN + full ctest**

- [ ] **Step 5: Commit**

```bash
git commit -m "feat(base): add config, logging and metrics library"
```

---

### Task 1: Contract 固定宽度安全截断

**Files:**
- Create: `ctp/include/ors/ctp/ContractText.h`
- Create: `ctp/src/ContractText.cpp`
- Create: `ctp/tests/ContractTextTest.cpp`
- Modify: `ctp/src/ContractSink.cpp`
- Modify: `ctp/src/RuntimeConfig.cpp`
- Modify: `ctp/tests/RuntimeConfigTest.cpp`
- Modify: `ctp/CMakeLists.txt`

**Interfaces:**
- `std::string fitContractField(std::string_view text, std::size_t maxBytes);`
- `void requireContractWidth(std::string_view value, std::size_t maxBytes, const std::string& key);`

- [ ] **Step 1: Write failing tests**
  - `fitContractField("你好世界", 7)` 不得返回半个 UTF-8 字符。
  - 超长 ASCII reason 截到 64 字节。
  - `RuntimeConfigTest`：`VENUE_ID` 33 字节、`ORS_ID` 33 字节、`ENVIRONMENT` 17 字节必须抛错。

- [ ] **Step 2: Run to verify RED**

```bash
ssh root@101.43.54.190 'cd /root/quantlink-ors && cmake --build build -j && ctest -R "ctp_contract_text|ctp_runtime_config" --output-on-failure'
```

- [ ] **Step 3: Implement**
  - `fitContractField` 按 UTF-8 起始字节向前回退，保证 `size <= maxBytes` 且不截断多字节字符。
  - `ContractSink` 对 `reason`、`venueId`、`orsId`、`environment`、`exchangeOrderId`、`tradeId`、`requestId` 等固定字段使用安全截断。
  - `RuntimeConfig::load` 校验 `VENUE_ID`/`ORS_ID`/`ENVIRONMENT` 的 contract 宽度。

- [ ] **Step 4: Verify GREEN**

```bash
ssh root@101.43.54.190 'cd /root/quantlink-ors && cmake --build build -j && ctest --test-dir build --output-on-failure'
```

- [ ] **Step 5: Commit**

```bash
git add ctp/include/ors/ctp/ContractText.h ctp/src/ContractText.cpp ctp/tests/ContractTextTest.cpp ctp/src/ContractSink.cpp ctp/src/RuntimeConfig.cpp ctp/tests/RuntimeConfigTest.cpp ctp/CMakeLists.txt
git commit -m "fix(ctp): safely fit contract fixed-width fields"
```

---

### Task 2: Aeron offer 重试与 transport health

**Files:**
- Create: `ctp/include/ors/ctp/OfferRetry.h`
- Create: `ctp/src/OfferRetry.cpp`
- Create: `ctp/tests/OfferRetryTest.cpp`
- Modify: `ctp/src/AeronPublisher.cpp`
- Modify: `ctp/include/ors/ctp/AeronPublisher.h`
- Modify: `ctp/src/ContractRuntime.cpp`
- Modify: `ctp/CMakeLists.txt`

**Interfaces:**
- `enum class OfferResult { Published, BackPressured, NotConnected, Closed };`
- `OfferResult offerWithRetry(const std::function<OfferResult()>& offer, std::chrono::milliseconds timeout);`

- [ ] **Step 1: Write failing test**
  - 前两次 `BackPressured`、第三次 `Published` 时返回 `Published`。
  - `NotConnected` 不重试，立即返回。
  - 超时返回最后一次失败。

- [ ] **Step 2: Run RED**

```bash
ssh root@101.43.54.190 'cd /root/quantlink-ors && cmake --build build -j && ctest -R ctp_offer_retry --output-on-failure'
```

- [ ] **Step 3: Implement**
  - `AeronFramePublisher::publish` 把 Aeron offer 返回值映射为 `OfferResult`，对 `BACK_PRESSURED` 使用有限重试。
  - `ContractRuntime` 维护 `transportHealthy_`；执行帧发布失败时拒绝新 OrderRequest/CancelRequest，并记录错误；恢复连接后重新允许。
  - 不把 `NotConnected` 当成功。

- [ ] **Step 4: Verify GREEN + full ctest**

- [ ] **Step 5: Commit**

```bash
git commit -m "fix(ctp): retry aeron offers and gate on transport health"
```

---

### Task 3: CTP session 可恢复断线

**Files:**
- Create: `ctp/include/ors/ctp/SessionRecovery.h`
- Create: `ctp/src/SessionRecovery.cpp`
- Create: `ctp/tests/SessionRecoveryTest.cpp`
- Modify: `ctp/src/CtpMdSession.cpp`
- Modify: `ctp/src/CtpTraderSession.cpp`
- Modify: `ctp/src/ContractRuntime.cpp`
- Modify: `ctp/CMakeLists.txt`

**Interfaces:**
- `bool canLoginAfterDisconnect(SessionState state);`
- `bool isTerminalSessionFailure(SessionState state);`

- [ ] **Step 1: Write failing tests**
  - `Disconnected` 允许重新登录；`Failed` 不是可重试状态。
  - `Failed` 被识别为终态失败。

- [ ] **Step 2: Run RED**

- [ ] **Step 3: Implement**
  - MD `onFrontConnected` 接受 `Disconnected`，重新登录并重发订阅。
  - Trader `onFrontConnected` 行为保持；runtime 在 `SessionState::Failed` 时返回非零，交给 supervisor 重启，不再僵尸。
  - `ContractRuntime` 断开时 epoch++，READY 只由 session 状态决定。

- [ ] **Step 4: Verify GREEN + ctest**

- [ ] **Step 5: Commit**

```bash
git commit -m "fix(ctp): recover md and fail closed on trader failure"
```

---

### Task 4: MD 旧 tick / 时间戳

**Files:**
- Create: `ctp/include/ors/ctp/MarketDataFreshness.h`
- Create: `ctp/src/MarketDataFreshness.cpp`
- Create: `ctp/tests/MarketDataFreshnessTest.cpp`
- Modify: `ctp/src/CtpMdSession.cpp`
- Modify: `ctp/src/ContractSink.cpp`
- Modify: `ctp/src/ContractRuntime.cpp`
- Modify: `ctp/CMakeLists.txt`

**Interfaces:**
- `bool isMarketDataStale(std::chrono::steady_clock::time_point last, std::chrono::steady_clock::time_point now, std::chrono::milliseconds limit);`
- `std::uint64_t ctpTimestampToNanos(const std::string& tradingDay, const std::string& updateTime, int millisec);`

- [ ] **Step 1: Write failing tests**
  - 超过 `staleAfter` 判定 stale。
  - `11:02:49.500` + tradingDay 转为 epoch nanos，非 0。
  - 未变化 tick 不重复发布。

- [ ] **Step 2: Run RED**

- [ ] **Step 3: Implement**
  - `CtpMdSession` 保存每合约最新 tick 的时间戳。
  - `ContractRuntime` 只在 `marketDataCount` 增加时发布；stale 时停止发布，并把 VenueSession 标为 DEGRADED/LOST。
  - `ContractSink` 写入真实 `exchangeTimestamp` / `ingestTimestamp`，不再写 0。

- [ ] **Step 4: Verify GREEN + ctest**

- [ ] **Step 5: Commit**

```bash
git commit -m "fix(ctp): stop stale md republish and carry timestamps"
```

---

### Task 5: 入站 identity 校验

**Files:**
- Create: `ctp/include/ors/ctp/InboundValidation.h`
- Create: `ctp/src/InboundValidation.cpp`
- Create: `ctp/tests/InboundValidationTest.cpp`
- Modify: `ctp/src/AeronCommandSubscriber.cpp`
- Modify: `ctp/src/ContractRuntime.cpp`
- Modify: `ctp/CMakeLists.txt`

**Interfaces:**
- `std::optional<std::string> validateInboundOrder(const OrderRequest& request, std::string_view venueId, std::string_view accountId, std::uint8_t environment);`
- `std::optional<std::string> validateInboundCancel(const CancelRequest& request, std::string_view venueId, std::string_view accountId, std::uint8_t environment);`

- [ ] **Step 1: Write failing tests**
  - 正确 account/venue/environment 通过。
  - account 不匹配、venue 不匹配、environment 不匹配分别拒绝并返回明确理由。

- [ ] **Step 2: Run RED**

- [ ] **Step 3: Implement**
  - `AeronCommandSubscriber` 解析 OrderRequest/CancelRequest 的 `environment`。
  - `ContractRuntime` 在调用 adapter 前校验；不匹配时发布本地 REJECTED，不发送 CTP。
  - L0 只认 venue account；不做业务账户映射。

- [ ] **Step 4: Verify GREEN + ctest**

- [ ] **Step 5: Commit**

```bash
git commit -m "fix(ctp): reject mismatched inbound account or venue"
```

---

### Task 6: account/position 查询节流

**Files:**
- Create: `ctp/include/ors/ctp/QueryCadence.h`
- Create: `ctp/src/QueryCadence.cpp`
- Create: `ctp/tests/QueryCadenceTest.cpp`
- Modify: `ctp/src/ContractRuntime.cpp`
- Modify: `ctp/src/RuntimeConfig.cpp`
- Modify: `ctp/tests/RuntimeConfigTest.cpp`
- Modify: `ctp/CMakeLists.txt`

**Interfaces:**
- `enum class QueryKind { Account, Position };`
- `QueryKind nextQuery(QueryKind current);`

- [ ] **Step 1: Write failing tests**
  - Account -> Position -> Account 交替。
  - `QUERY_INTERVAL_SECONDS=0` 拒绝；最小安全值 1s。

- [ ] **Step 2: Run RED**

- [ ] **Step 3: Implement**
  - runtime 每轮只发一个 query，account/position 交替。
  - 保留 `QUERY_INTERVAL_SECONDS`，最小 1s。
  - 查询失败记录并继续，不影响 READY。

- [ ] **Step 4: Verify GREEN + ctest**

- [ ] **Step 5: Commit**

```bash
git commit -m "fix(ctp): space account and position queries"
```

---

### Task 7: 旧 front 回报测试补齐

**Files:**
- Modify: `ctp/tests/CtpAdapterTest.cpp`

- [ ] **Step 1: Write failing tests**
  - 旧 front 的 `OnRspOrderAction` / `OnErrRtnOrderAction` 不发布撤单拒绝。
  - 旧 front 的 `OnRspOrderInsert` 不发布拒单。
  - 当前 front 的对应回报仍然发布。

- [ ] **Step 2: Run RED**
  - 如果实现已正确，测试应先暴露缺口；若测试直接通过，说明该行为已存在，把测试保留为回归。

- [ ] **Step 3: Fix only if RED**
  - 在 identity guard 缺失的 callback 上补检查，不改变现有 policy。

- [ ] **Step 4: Verify GREEN + ctest**

- [ ] **Step 5: Commit**

```bash
git commit -m "test(ctp): cover stale front cancel and insert reports"
```

---

### Task 8: 全套验证与 live 回归

- [ ] **Step 1: Build/test on 190**

```bash
ssh root@101.43.54.190 'cd /root/quantlink-ors && cmake --build build -j && ctest --test-dir build --output-on-failure'
```

- [ ] **Step 2: Live lifecycle**

```bash
ssh root@101.43.54.190 'cd /root/quantlink-ors && stdbuf -oL -eL timeout -s KILL 420 ./build/ctp/ors_ctp_smoke --config /opt/quantlink-fin/run/ors-simnow.cfg --mode lifecycle-smoke --instruments au2612 --price 735 --fill-price 775 --qty 1 --live'
```

Expected: 单登录，cancel-reject + fill + account/position snapshots，无 `FAILED`。

- [ ] **Step 3: Runtime startup**

```bash
ssh root@101.43.54.190 'cd /root/quantlink-ors && stdbuf -oL -eL timeout -s KILL 360 ./build/ctp/ors_ctp --config /opt/quantlink-fin/run/ors-simnow.cfg'
```

Expected: CTP/MD READY；无 reconciliation gate；无崩溃。

- [ ] **Step 4: Commit/push if any doc or code changed**
