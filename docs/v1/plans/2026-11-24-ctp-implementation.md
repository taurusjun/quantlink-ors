# ORS CTP 对接实施计划

> **面向 agentic workers：** 必须使用子技能 `superpowers:subagent-driven-development`（推荐）或 `superpowers:executing-plans`，按任务逐步执行。步骤使用 `- [ ]` 复选框跟踪。

**目标：** 在 `quantlink-ors` 中实现独立 C++ CTP 子项目，先用 SimNow 完成登录、查询、行情、下单/撤单、回报、账户/持仓，再输出 L0 contract 0.2.0。

**架构：** 一个 ORS 进程绑定一个 CTP/SimNow 账户；TraderApi 和 MdApi 各一个 session；内部 `CtpAdapter` 归一化 CTP 回调；`ContractSink` 用 contract 0.2.0 的 C++ SBE/Aeron 编码发布。146 China ORS 只作参考，不复用其 2014 头文件和 SHM 协议。

**技术栈：** C++20、CMake 3.25+、CTP 6.7.x（SimNow 支持版本，构建时 pin）、SBE 1.35.6 C++ codegen、Aeron C++ client、CTest。

**Spec：** `docs/v1/2026-11-24-l0-ors-ctp-design.md`

## Global Constraints

```text
1 ORS = 1 CTP/SimNow 账户
TraderApi 1 个 session，MdApi 1 个 session
CTP SDK 不入库，通过 CTP_HOME 或 third_party/ctp 提供
CTP 版本构建时 pin，不使用 146 2014 头文件
SimNow 密码/AppID/AuthCode 不入库
L0 contract 固定 0.2.0
orderRef 由 L2 分配，CTP adapter 映射到 CTP OrderRef
CTP ModifyRequest 第一版明确 rejected
行情 20 档，CTP 不足 5 档补零
PositionSnapshot / AccountSnapshot 必需
SHFE/INE 平今/平昨由 L2 ExecutionOffsetPolicy 解析，CTP ORS 只做 1:1 offsetFlag 映射
非 SHFE/INE 的显式 CLOSE_TODAY/CLOSE_YESTERDAY 必须由 L2 拒绝，CTP ORS 不改写
CTP error 51 不能作为 ORS 自动拆平今/平昨的依据
快照 snapshotTimestamp 使用 epoch nanoseconds，staleAfterMs 使用毫秒
sourceSessionId 默认每次启动生成；sourcePublicationEpoch 可配置，重连时递增
CTP ErrorMsg/StatusMsg 在写入 L1 前从 GB18030 转 UTF-8
只在 190 构建和测试
```

## Review Focus

- `CTP_HOME` 缺失或架构不匹配时，配置阶段必须明确失败，不能编译到一半。
- SimNow 登录缺少 AuthCode/AppID 或结算确认未完成时，不能进入 READY。
- CTP `OrderRef` 超过 12 位或重复时，必须拒绝，不能覆盖已有订单。
- CTP 断线重连后旧 session 的回报不能写入新 epoch。
- CTP 只有 5 档行情时，第 6–20 档必须是零，不能复制第 5 档。
- CTP offset 只验证 1:1 映射；SHFE/INE 规则和跨交易所拒绝逻辑必须在 L2 测试。
- 任何日志/异常都不能输出密码、AuthCode 或完整账户配置。

---

## 文件结构

```text
quantlink-ors/
  CMakeLists.txt
  cmake/FindCTP.cmake
  ctp/
    CMakeLists.txt
    include/ors/ctp/
      CtpConfig.h
      CtpSession.h
      CtpAdapter.h
      CtpOrderRefMap.h
      CtpReconciler.h
      ContractSink.h
    src/
      CtpConfig.cpp
      CtpTraderSession.cpp
      CtpMdSession.cpp
      CtpAdapter.cpp
      CtpOrderRefMap.cpp
      CtpReconciler.cpp
      ContractSink.cpp
      main.cpp
    tests/
      CtpConfigTest.cpp
      CtpOrderRefMapTest.cpp
      CtpAdapterTest.cpp
      CtpReconcilerTest.cpp
    configs/
      simnow.example.cfg
  tools/
    gen_contract_cpp.sh
  docs/
```

---

### Task 1: CMake 骨架和 CTP SDK 发现

**Files:**
- Create: `CMakeLists.txt`
- Create: `cmake/FindCTP.cmake`
- Create: `ctp/CMakeLists.txt`
- Create: `ctp/tests/CtpConfigTest.cpp`
- Create: `.gitignore`

**Interfaces:**
- `FindCTP` 定义 `CTP_INCLUDE_DIR`、`CTP_TRADER_LIBRARY`、`CTP_MD_LIBRARY`
- CMake option `CTP_HOME`
- 目标 `ors_ctp_core`、`ors_ctp`

- [ ] **Step 1: 写失败的配置测试**

创建 `CtpConfigTest.cpp`，断言缺少 `CTP_HOME` 时 CMake 配置失败，错误信息包含 `CTP_HOME`。

- [ ] **Step 2: 运行 190 上的配置检查**

```bash
cmake -S . -B build -DCTP_HOME=/nonexistent
```

Expected: FAIL，错误信息明确包含 `CTP_HOME`。

- [ ] **Step 3: 实现 CMake 骨架**

`CMakeLists.txt`：

```cmake
cmake_minimum_required(VERSION 3.25)
project(quantlink_ors LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_subdirectory(ctp)
```

`ctp/CMakeLists.txt`：

```cmake
find_package(CTP REQUIRED)
add_library(ors_ctp_core ...)
add_executable(ors_ctp src/main.cpp)
target_link_libraries(ors_ctp PRIVATE ors_ctp_core CTP::trader CTP::md)
enable_testing()
add_executable(ctp_config_test tests/CtpConfigTest.cpp)
add_test(NAME ctp_config COMMAND ctp_config_test)
```

- [ ] **Step 4: 验证 190 构建**

```bash
cmake -S . -B build -DCTP_HOME=/opt/ctp
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Expected: PASS。

- [ ] **Step 5: 提交**

```bash
git add CMakeLists.txt cmake ctp .gitignore
git commit -m "feat(ctp): add CMake skeleton and SDK discovery"
```

---

### Task 2: SimNow Trader 登录和 instrument 查询

**Files:**
- Create: `ctp/include/ors/ctp/CtpConfig.h`
- Create: `ctp/src/CtpConfig.cpp`
- Create: `ctp/include/ors/ctp/CtpSession.h`
- Create: `ctp/src/CtpTraderSession.cpp`
- Create: `ctp/configs/simnow.example.cfg`
- Test: `ctp/tests/CtpConfigTest.cpp`

**Interfaces:**
- `CtpConfig::load(path, env) -> CtpConfig`
- `CtpConfig` 字段：`brokerId/investorId/password/appId/authCode/tradeFront/mdFront/traderFlowPath/mdFlowPath`
- `CtpTraderSession::start()`, `state()`, `queryInstruments()`

- [ ] **Step 1: 写配置测试**

断言：

```text
缺少 INVESTOR_ID -> 抛错
缺少 PASSWORD -> 抛错
敏感值从 env 覆盖文件
日志不包含 password/authCode
```

- [ ] **Step 2: 实现配置加载**

`CtpConfig::load` 读取 key=value；敏感项优先从环境变量：

```text
CTP_PASSWORD
CTP_APP_ID
CTP_AUTH_CODE
```

- [ ] **Step 3: 实现 Trader 登录状态机**

```text
DISCONNECTED
  -> CONNECTING
  -> AUTHENTICATING
  -> LOGGING_IN
  -> SETTLEMENT_CONFIRMING
  -> QUERYING_INSTRUMENTS
  -> READY
```

回调：

```text
OnFrontConnected -> ReqAuthenticate
OnRspAuthenticate -> ReqUserLogin
OnRspUserLogin -> ReqSettlementInfoConfirm
OnRspSettlementInfoConfirm -> ReqQryInstrument
OnRspQryInstrument(bIsLast=true) -> READY
```

- [ ] **Step 4: SimNow smoke**

190：

```bash
./build/ctp/ors_ctp_smoke --config /opt/ors/configs/simnow.cfg --mode login-smoke
```

Expected: 输出 `READY` 和 instrument 数量，退出码 0。

- [ ] **Step 5: 提交**

```bash
git add ctp/include/ors/ctp/CtpConfig.h ctp/src/CtpConfig.cpp \
  ctp/include/ors/ctp/CtpSession.h ctp/src/CtpTraderSession.cpp \
  ctp/configs/simnow.example.cfg ctp/tests/CtpConfigTest.cpp
git commit -m "feat(ctp): add SimNow trader login and instrument query"
```

---

### Task 3: MdApi 和 20 档行情

**Files:**
- Create: `ctp/src/CtpMdSession.cpp`
- Create: `ctp/tests/CtpMdSessionTest.cpp`
- Modify: `ctp/src/main.cpp`

**Interfaces:**
- `CtpMdSession::start()`, `subscribe(contracts)`, `state()`
- 输出 `MarketData` 的 20 档数组；1–5 来自 CTP，6–20 补零。

- [ ] **Step 1: 写行情映射测试**

```cpp
auto md = mapDepthMarketData(fakeTick);
ASSERT_EQ(20, md.bidPrices.size());
ASSERT_EQ(0.0, md.bidPrices[5]);
ASSERT_EQ(0, md.bidQtys[19]);
```

- [ ] **Step 2: 实现 Md 登录/订阅**

```text
RegisterFront
Init
OnFrontConnected -> ReqUserLogin
OnRspUserLogin -> SubscribeMarketData
OnRtnDepthMarketData -> mapDepthMarketData
```

- [ ] **Step 3: 190 SimNow 行情 smoke**

```bash
./build/ctp/ors_ctp_smoke --config /opt/ors/configs/simnow.cfg --mode md-smoke
```

Expected: 至少收到一条行情，20 档结构完整。

- [ ] **Step 4: 提交**

```bash
git add ctp/src/CtpMdSession.cpp ctp/tests/CtpMdSessionTest.cpp ctp/src/main.cpp
git commit -m "feat(ctp): subscribe market data with 20-level padding"
```

---

### Task 4: 账户/持仓/订单查询

**Files:**
- Create: `ctp/src/CtpQueryService.cpp`
- Create: `ctp/include/ors/ctp/CtpQueryService.h`
- Create: `ctp/tests/CtpQueryServiceTest.cpp`

**Interfaces:**
- `CtpQueryService::queryAccount()`, `queryPositions()`, `queryOrders()`, `queryTrades()`
- 查询完成回调 `onSnapshot(...)`

- [ ] **Step 1: 写查询聚合测试**

断言同一 `snapshotId` 的持仓行使用同一 `tradingDay`/`snapshotTimestamp`。

- [ ] **Step 2: 实现查询**

```text
ReqQryTradingAccount -> AccountSnapshot
ReqQryInvestorPosition -> PositionSnapshot[]
ReqQryOrder / ReqQryTrade -> ReconciliationSnapshot
```

- [ ] **Step 3: 190 SimNow 查询 smoke**

```bash
./build/ctp/ors_ctp_smoke --config /opt/ors/configs/simnow.cfg --mode query-smoke
```

Expected: 账户余额/持仓/订单查询返回；输出不包含密码。

- [ ] **Step 4: 提交**

```bash
git add ctp/src/CtpQueryService.cpp ctp/include/ors/ctp/CtpQueryService.h ctp/tests/CtpQueryServiceTest.cpp
git commit -m "feat(ctp): query account positions orders and trades"
```

---

### Task 5: 下单、撤单、回报

**Files:**
- Create: `ctp/include/ors/ctp/CtpOrderRefMap.h`
- Create: `ctp/src/CtpOrderRefMap.cpp`
- Create: `ctp/src/CtpAdapter.cpp`
- Create: `ctp/tests/CtpOrderRefMapTest.cpp`
- Create: `ctp/tests/CtpAdapterTest.cpp`

**Interfaces:**
- `CtpOrderRefMap::ctpRef(uint64_t orderRef) -> std::string`
- `CtpOrderRefMap::resolve(frontId, sessionId, ctpRef) -> uint64_t`
- `CtpAdapter::onOrderRequest(...)`, `onCancelRequest(...)`
- `CtpAdapter` 不实现 ModifyRequest；返回拒绝。

- [ ] **Step 1: 写 orderRef 测试**

```text
orderRef 1 -> "1"
orderRef 999999999999 -> 12 位合法
orderRef 1000000000000 -> 拒绝
重复 orderRef -> 拒绝
```

- [ ] **Step 2: 实现订单映射和回调**

```text
ReqOrderInsert -> OnRspOrderInsert / OnRtnOrder
ReqOrderAction(Delete) -> OnRspOrderAction / OnRtnOrder
OnRtnTrade -> Trade
ModifyRequest -> ORDER_ERROR + "modify unsupported by CTP"
```

- [ ] **Step 3: 写 adapter 测试**

覆盖：

```text
insert accepted
cancel accepted
CTP error -> OrderUpdate REJECTED
unknown orderRef -> buffer/UNKNOWN
modify -> rejected unsupported
```

- [ ] **Step 4: SimNow 订单 smoke**

在允许交易的时段：

```bash
./build/ctp/ors_ctp_smoke --config /opt/ors/configs/simnow.cfg --mode order-smoke
```

Expected: 下单、撤单、OrderUpdate/Trade 完整。

- [ ] **Step 5: 提交**

```bash
git add ctp/include/ors/ctp/CtpOrderRefMap.h ctp/src/CtpOrderRefMap.cpp \
  ctp/src/CtpAdapter.cpp ctp/tests/CtpOrderRefMapTest.cpp ctp/tests/CtpAdapterTest.cpp
git commit -m "feat(ctp): route orders and execution reports"
```

---

### Task 6: Contract 0.2.0 C++ 输出

**Files:**
- Create: `tools/gen_contract_cpp.sh`
- Create: `ctp/include/ors/ctp/ContractSink.h`
- Create: `ctp/src/ContractSink.cpp`
- Modify: `ctp/CMakeLists.txt`
- Test: `ctp/tests/ContractSinkTest.cpp`

**Interfaces:**
- `ContractSink::publishMarketData(...)`
- `ContractSink::publishOrderUpdate(...)`
- `ContractSink::publishTrade(...)`
- `ContractSink::publishSnapshot(...)`
- `ContractSink::publishSession(...)`

- [ ] **Step 1: 生成 C++ SBE**

```bash
./tools/gen_contract_cpp.sh \
  --contract /opt/quantlink-venue-contract \
  --version 0.2.0
```

生成到 `build/generated/contract/`，不提交生成物。

- [ ] **Step 2: 写 contract sink 测试**

断言：

```text
OrderUpdate/Trade 共用 execution stream
sequence 单调
ModifyRequest 被拒绝
MarketData 20 档
VenueSession lastSeqByStream 正确
```

- [ ] **Step 3: 接 Aeron**

使用 Aeron C++ client；stream 配置来自 ORS cfg，默认值来自 contract `AeronStreams`。

- [ ] **Step 4: 190 集成测试**

启动 SimVenue 或 FastTrade AccessClient 订阅，验证：

```text
VenueSession READY
MarketData
AccountSnapshot / PositionSnapshot
OrderUpdate / Trade
```

- [ ] **Step 5: 提交**

```bash
git add tools/gen_contract_cpp.sh ctp/include/ors/ctp/ContractSink.h \
  ctp/src/ContractSink.cpp ctp/CMakeLists.txt ctp/tests/ContractSinkTest.cpp
git commit -m "feat(ctp): publish L0 contract 0.2.0"
```

---

### Task 7: 重连和对账

**Files:**
- Create: `ctp/include/ors/ctp/CtpReconciler.h`
- Create: `ctp/src/CtpReconciler.cpp`
- Create: `ctp/tests/CtpReconcilerTest.cpp`

**Interfaces:**
- `CtpReconciler::onReconnect()`
- `CtpReconciler::reconcile(orders, trades, positions)`

- [ ] **Step 1: 写重连测试**

```text
旧 session 的迟到回报被丢弃
查询回来 FILLED -> 更新终态
查询不到且无回报 -> UNKNOWN
断线期间不重复下单
```

- [ ] **Step 2: 实现重连流程**

```text
DEGRADED
  -> 重连/认证/登录/结算
  -> epoch++
  -> query order/trade/position
  -> reconcile
  -> READY 或保持 DEGRADED
```

- [ ] **Step 3: 190 断线 smoke**

手动断开网络或重启前置，验证：

```text
不重复下单
旧回报不写入新 epoch
对账后恢复
```

- [ ] **Step 4: 提交**

```bash
git add ctp/include/ors/ctp/CtpReconciler.h ctp/src/CtpReconciler.cpp ctp/tests/CtpReconcilerTest.cpp
git commit -m "feat(ctp): reconcile after reconnect"
```

---

### Task 8: SimNow 190 验收

**Files:**
- Create: `docs/v2/2026-11-24-ctp-acceptance.md`
- Modify: `README.md`

**验收命令：**

```bash
cmake -S /opt/quantlink-ors -B /opt/quantlink-ors/build -DCTP_HOME=/opt/ctp
cmake --build /opt/quantlink-ors/build -j
ctest --test-dir /opt/quantlink-ors/build --output-on-failure
```

SimNow smoke：

```bash
/opt/quantlink-ors/build/ctp/ors_ctp_smoke --config /opt/ors/configs/simnow.cfg --mode login-smoke
/opt/quantlink-ors/build/ctp/ors_ctp_smoke --config /opt/ors/configs/simnow.cfg --mode query-smoke
/opt/quantlink-ors/build/ctp/ors_ctp_smoke --config /opt/ors/configs/simnow.cfg --mode md-smoke
```

Expected:

```text
CTP Trader/MD 登录成功
instrument 查询成功
行情 20 档
账户/持仓快照
下单/撤单/成交回报
contract 0.2.0 消息可被 L1 消费
```

提交：

```bash
git add docs/v2/2026-11-24-ctp-acceptance.md README.md
git commit -m "docs: record CTP SimNow acceptance"
```

---

## 执行说明

- 所有构建/测试在 190 上执行；本机只编辑/提交。
- CTP SDK 版本由 `ctp/CMakeLists.txt` 和 `docs` 记录，不把 `.so` 提交。
- 第一版不实现 CTP ModifyRequest；收到即拒绝。
- SimNow 账户信息用环境变量或受控配置文件，不入库。
