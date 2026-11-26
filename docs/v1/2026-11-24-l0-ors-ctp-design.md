# L0 ORS CTP 对接方案

- 日期：2026-11-24
- 状态：APPROVED（2026-11-24）
- 范围：`quantlink-ors` 新项目、`ctp` 子项目、SimNow 测试、与 L0 contract 的边界
- 关联：`quantlink-venue-contract` 0.2.0、`quantlink-fasttrade/docs/v2/003-l0-ors-venue.md`

---

## 1. 决策

新建独立仓库：

```text
quantlink-ors/
  ctp/
```

第一版只做：

```text
CTP TraderApi + MdApi
  -> CTP 适配层
  -> L0 contract（SBE + Aeron）
```

不复用 146 的 `China/` ORS 源码作为运行时：

```text
146 China ORS 是 2014 年 CTP 头文件 + SHM + 自定义 RequestMsg/ResponseMsg
SimNow 需要当前 CTP SDK
L0 需要 0.2.0 contract（orderRef / 20 档 / ModifyRequest）
```

146 `China/` 只作为 CTP 回调、订单字段和重连逻辑的参考，不直接复制。

---

## 2. Goals

```text
1. 独立 C++ CTP ORS 项目
2. 一个 ORS = 一个 SimNow/CTP 账户 + 一个 Trader session + 一个 Md session
3. 直接产出 L0 contract 0.2.0 消息
4. 用 SimNow 账户完成登录、查询、行情、下单、撤单、成交、持仓验收
5. 运行在 190（Rocky Linux 9 / x86_64）
6. 不把密码、AppID、AuthCode 写进仓库
7. 断线重连后由 L2/LocalAgent/ReadModel 对账；ORS 不建立订单状态，不查询 CTP 订单/成交
```

非目标：

```text
不做多账户路由
不做 CTP 之外第二家柜台
不在第一版实现 CTP 原子改单（CTP 没有）
不修改 146 源码
不把 CTP 原生结构体暴露给 L0 contract
```

---

## 3. 方案比较

### 方案 A：复用 146 China ORS

```text
China/ORSServer + MdSpi + TraderSpi
  -> 改造成 L0 contract
```

拒绝原因：

```text
CTP 头文件是 2014 版本，SimNow 当前 SDK 不兼容
依赖 146 的 SHM/RequestMsg/ResponseMsg 和旧 logger/RMS
改造成本高于新写一个薄适配层
```

### 方案 B：新 C++ CTP 子项目（选定）

```text
SimNow/CTP
  -> ctp adapter
  -> contract C++ codec
  -> Aeron
```

优点：

```text
CTP 版本可控
只保留 L0 需要的字段
可以直接按 contract 0.2.0 验收
```

代价：

```text
需要 C++ SBE 生成 + Aeron C++ 客户端
需要自己实现订单映射/重连
```

### 方案 C：CTP sidecar + Java contract bridge

```text
C++ CTP sidecar -> TCP/JSON -> Java bridge -> contract
```

拒绝原因：

```text
多一个进程和协议层
不符合“contract 直接贯穿 L0”
排障链更长
```

---

## 4. 总体结构

```text
                 SimNow / CTP
        ┌────────────┴────────────┐
        │                         │
  CThostFtdcMdApi          CThostFtdcTraderApi
        │                         │
   CtpMdSession              CtpTraderSession
        │                         │
        └──────────┬──────────────┘
                   │
              CtpAdapter
              ├─ CtpConfig
              ├─ CtpInstrumentMap
              ├─ CtpOrderRefMap
              └─ CtpClock
                   │
             ContractSink
             ├─ SBE C++ codec
             └─ Aeron C++ publication
                   │
                 L1 IPC
```

进程：

```text
quantlink-ors-ctp
  = 1 个 CTP account
  + 1 个 TraderApi
  + 1 个 MdApi
  + 1 个 contract sink
```

不把 CTP TraderApi/MdApi 暴露到 L1。

---

## 5. 子项目边界

### `ctp`

负责：

```text
CTP 连接/登录/结算确认
Instrument 查询
行情订阅
订单插入/撤单
订单/成交回报
账户/持仓查询
CTP -> L0 字段映射
重连后重新发布 session；不查询/不维护订单状态
```

不负责：

```text
策略
订单记录/订单状态权威（归 L2 TradingChannel / OrderRegistry）
订单读模型（归 L4 LocalAgent / L5 ReadModel）
```
账户路由
GUI
PnL 计算
STP（自成交检查）
SHFE/INE 平今/平昨的选择（由 L2 解析后只传最终 offsetFlag；L2 不拆单）
```

### `contract`

由 `quantlink-venue-contract` schema 生成 C++ codec，不手写二套字段。

第一版固定：

```text
contract 0.2.0
orderRef
accountId = venueAccountId = CTP InvestorID/账户绑定
20 档行情
PositionSnapshot / AccountSnapshot 必需
OrderUpdate / Trade 共用 execution stream
```

---

## 6. CTP 会话

### 6.1 启动顺序

```text
1. 读取配置
2. 创建 TraderApi（flow 目录）
3. RegisterFront
4. Init
5. OnFrontConnected
6. ReqAuthenticate
7. ReqUserLogin
8. ReqSettlementInfoConfirm
9. ReqQryInstrument
10. 登录 MdApi + ReqUserLogin
11. 订阅配置合约
12. publish VenueSession READY
```

### 6.2 session 状态

```text
DISCONNECTED
  -> CONNECTING
  -> AUTHENTICATING
  -> LOGGING_IN
  -> SETTLEMENT_CONFIRMING
  -> QUERYING_INSTRUMENTS
  -> READY
  -> DEGRADED
  -> CLOSED
```

只有 `READY` 接受新单。

### 6.3 断线

```text
OnFrontDisconnected
  -> DEGRADED
  -> 停止新单
  -> 重连后重新认证/登录/结算确认
  -> sourceSessionId/sourcePublicationEpoch 递增
  -> 查询 positions 发布快照；订单对账由 L2/LocalAgent/ReadModel 负责
```

不猜丢失回报。

---

### 6.3 sourceSessionId / sourcePublicationEpoch / stream IDs

```text
sourceSessionId        runtime 启动标识；未配置时自动生成
sourcePublicationEpoch  同一 runtime 的发布代数；配置项必填
stream IDs            由 STREAM_BASE 按固定布局整组生成
```

真实重连时必须递增 `sourcePublicationEpoch` 并重新发布 `VenueSession`；旧 epoch
的 OrderUpdate/Trade 必须丢弃。runtime 断线发布 LOST，重连后只查询 positions
完成快照，不建立订单状态。

已知残余风险（当前 SimNow 阶段暂时接受，实盘前必须处理）：runtime 目前会把
CTP 回调统一盖成当前 epoch，若 SDK 在重连后仍投递旧 front 回报，旧 FILLED/Trade
可能被当成新事件。详见 `docs/v2/2026-11-24-ctp-acceptance.md` §7.1。

当前已加一层连接代次过滤：`CtpAdapter` 用登录返回的 `FrontID` / `SessionID`
核对 `OnRtnOrder`、撤单回报和已绑定订单的 `OnRtnTrade`；旧 front 的回报直接丢弃。
代码位置和覆盖见 `CtpAdapterTest::checkOldSessionReportsDropped`。如果新旧登录
拿到相同的 `FrontID` / `SessionID`，仅靠这层仍无法区分，这是剩余风险。

成交事件不重放：CTP `OnRtnTrade` 按 at-most-once 处理，L0 不按 `tradeId` 去重。
Aeron 发布使用 `aeron:ipc`，没有 Archive/replay；`query-smoke` 的
`TradeSnapshot` 仅用于诊断，不进入 runtime 订单状态。如果未来引入
Aeron Archive 或消费端重放，由 L2 按契约唯一键
`(venueId, accountId, tradingDay, tradeId)` 做幂等，不在 L0 维护第二套去重集合。

## 7. 订单与 ID

CTP 字段：

```text
OrderRef     char[13]
FrontID      int
SessionID    int
OrderSysID   char[21]
ExchangeID
InstrumentID
```

contract 字段：

```text
orderRef     uint64
requestId    string
accountId    string
instrumentId string
exchange     string
```

规则：

```text
1. L0 contract 的 orderRef 由 L2/TradingChannel 分配
2. CTP adapter 把 orderRef 格式化为 CTP OrderRef（最多 12 位数字）
3. adapter 维护 orderRef <-> (FrontID, SessionID, OrderRef, OrderSysID)
4. 超出 12 位或非法 orderRef 直接拒绝
5. 重连后 ORS 不查询订单/成交；对账由 L2/LocalAgent/ReadModel 负责
```

CTP 的 `OrderSysID` / `TradeID` 是固定宽度 `char` 数组，实际值可能出现前导空格
（例如 `"      102465"`）。L0 映射时去掉前后空格，contract 中只保留实际 ID。

L0 不设置“空 `tradeId` 直接丢弃”的分支。`TradeID` 是 CTP `OnRtnTrade` 的成交编号，
正常必填；空值属于柜台/API 异常。静默丢成交会掩盖问题，因此保留原值交给契约/
下游显式失败。L0 也不按 `tradeId` 去重：CTP 私流按 at-most-once 处理，ORS 没有
Aeron Archive/replay；如果未来引入重放，由 L2 按契约唯一键
`(venueId, accountId, tradingDay, tradeId)` 做幂等。

### 7.1 平今/平昨（offsetFlag）

CTP ORS 只做最终 `offsetFlag` 的 1:1 映射，不判断交易所，不拆分，不改写：

```text
OPEN              -> THOST_FTDC_OF_Open
CLOSE             -> THOST_FTDC_OF_Close
CLOSE_TODAY       -> THOST_FTDC_OF_CloseToday
CLOSE_YESTERDAY   -> THOST_FTDC_OF_CloseYesterday
```

SHFE/INE 的语义由 L2 决定：

```text
CLOSE 在 SHFE/INE 表示平昨；平今必须显式 CLOSE_TODAY。
只发 CLOSE 可能静默平掉昨仓，也可能因昨仓不足被 CTP 拒绝（错误码 51）。
CTP error 51 只作为失败回报，不能反过来驱动 ORS 猜 split。
```

非 SHFE/INE 交易所的显式 `CLOSE_TODAY` / `CLOSE_YESTERDAY` 由 L2 拒绝；
CTP ORS 不替 L2 做交易所规则校验，也不把 `CLOSE_TODAY` 改写成 `CLOSE`。

成交回报的 `ExchangeID` 可能为空：`CtpAdapter` 用该笔订单的逐单 exchange 兜底，
runtime 不使用全局配置 exchange，避免多交易所账户把成交标错。

CTP 头文件里有 `THOST_FTDC_AF_Modify` 常量，但当前 CTP API 文档明确：

```text
ReqOrderAction.ActionFlag 只支持 THOST_FTDC_AF_Delete
不支持 Modify
```

参考：CTP 6.7.11 `ReqOrderAction` / `OnErrRtnOrderAction` 文档；146 的 China ORS 也只使用 `THOST_FTDC_AF_Delete`，没有改单实现。

因此：

```text
ModifyRequest -> REJECTED，reason="modify unsupported by CTP"
```

第一版不实现 cancel+new 冒充 modify；客户端需要改单时显式 CANCEL + NEW。

> 这要求 contract 增加一个明确的 unsupported 错误码；在 contract 版本确认前，先用 `errorCode=0` + reason，不伪装成功。

---

## 8. 行情

CTP MdApi：

```text
SubscribeMarketData
OnRtnDepthMarketData
```

映射：

```text
InstrumentID -> ContractKey
ExchangeID   -> exchange
BidPrice1..5 / BidVolume1..5
AskPrice1..5 / AskVolume1..5
```

L0 contract 要求 20 档：

```text
CTP 只有 5 档
第 6..20 档 price=0 / qty=0
不重复、不伪造
```

行情序号：

```text
sourceSessionId + sourcePublicationEpoch + seq
```

---

## 9. 账户与持仓

启动和定时查询：

```text
ReqQryTradingAccount
ReqQryInvestorPosition
ReqQryInvestorPositionDetail
```

`query-smoke` 的 `ReqQryOrder` / `ReqQryTrade` 仅保留为诊断工具，runtime 不调用，
也不把结果写入订单状态。

发布：

```text
AccountSnapshot
PositionSnapshot
```

规则：

```text
accountId = CTP InvestorID / ORS venue account
snapshotId / snapshotTimestamp / staleAfterMs 必须完整
snapshotTimestamp 使用 epoch nanoseconds；staleAfterMs 使用毫秒
sourceSessionId 默认每次启动生成；sourcePublicationEpoch 可配置，重连时递增
查询失败 -> session DEGRADED，不伪造空快照
快照过期 -> 消费方按 staleAfterMs 判断
```

---

## 10. 订单/成交回报

CTP 回调：

```text
OnRspOrderInsert
OnRspOrderAction
OnRtnOrder
OnRtnTrade
OnErrRtnOrderInsert
OnErrRtnOrderAction
```

映射到 L0：

```text
OrderUpdate.status
  NEW / ACK / PARTIAL / FILLED / CANCELED / REJECTED
CancelState
  NONE / REQUESTED / REJECTED
errorCode / reason
```

回报必须带：

```text
orderRef
exchangeOrderId (OrderSysID)
sequence
sourceSessionId / sourcePublicationEpoch
```

未知 orderRef 不再进入 pending/reconciliation buffer。ORS 不做订单查询，也没有消费者；
按公司 ORS 的行为，未知回报直接丢弃并记录错误，避免无界内存和 sequence gap。

终态订单按公司 ORS 的 `RemoveFromMap` 语义处理：

```text
InsertRejected / Rejected
Canceled
成交累计量达到 orderQty -> Filled
```

发布终态 OrderUpdate（以及成交 Trade）后，立即从 `orders_` 和 live OrderRef 映射删除；
不保留 tombstone，不在日内复用 orderRef。`orderRef` 的唯一性由 L2 保证。

撤单拒绝：

```text
CTP 通过 OnRspOrderAction（前台拒绝）和 OnErrRtnOrderAction（交易所拒绝）返回；
同一次撤单请求可能两个回调都到，L0 按一次 cancel attempt 只发布一次
cancelState=REJECTED，订单 status 保持 REJECTED 前的工作状态（ACK/PARTIAL）。
OnRtnOrder 的 OrderSubmitStatus=CancelRejected 也走同一去重标记。
```

`ors_ctp_smoke --mode cancel-reject-smoke --live` 会挂一笔远离市价的单，发送一个
FrontID/SessionID 不匹配的 `ReqOrderAction`，验证真实 CTP 撤单拒绝，然后再正常
撤单清理。它不通过 `CtpAdapter::onCancelRequest` 伪造请求。

---

## 11. 重连与对账

ORS 不建立订单状态，也不查询 CTP 订单/成交。重连只负责：

```text
1. 登录/结算确认
2. 发布新的 VenueSession / epoch
3. 查询 positions 发布快照
4. 订单对账交给 L2 TradingChannel / OrderRegistry；读模型由 L3 FactPublisher / L4 LocalAgent / L5 ReadModel 生成
```

L2 的 `OrderRegistry` 是交易状态权威；L3 `FactPublisher` 发布订单/成交事实，
L4 `LocalAgent` 采集转发，L5 `ReadModel` 重建 open/closed 订单读模型。
v2 的 `DataService` 不是 v3 运行时组件；它只作为历史参考。

ORS 只保留临时 `orderRef -> FrontID/SessionID/OrderSysID` 路由信息，
用于撤单寻址和迟到回报识别。

---

## 12. Contract 编码

C++ 采用：

```text
SBE schema: quantlink-venue-contract 0.2.0
生成语言: C++（由 SBE tool 生成）
transport: Aeron C++ client
```

不在 ORS 手写第二套 SBE 结构。

需要增加：

```text
quantlink-ors/tools/gen_contract_cpp.sh
  -> 从固定 contract tag/schema 生成 C++ codec
```

生成代码必须 pin contract 版本，不能跟随 `main` 漂移。

---

## 13. 配置

配置文件示例（不提交真实密码）：

```ini
ORS_ID = ors-ctp-simnow-01
ENVIRONMENT = SHADOW
VENUE_ID = SIMNOW
EXCHANGE = SHFE

AERON_DIR = /var/lib/quantlink-fin/aeron
STREAM_BASE = 9000
SOURCE_PUBLICATION_EPOCH = 1
# SOURCE_SESSION_ID = 0        ; 0/省略时启动生成
# QUERY_INTERVAL_SECONDS = 1

BROKER_ID = 9999
INVESTOR_ID = <simnow-investor>
PASSWORD = <from-env>
APP_ID = simnow_client_test
AUTH_CODE = 0000000000000000

TRADE_FRONT = tcp://...
MD_FRONT = tcp://...

TRADER_FLOW_PATH = state/ctp/trader
MD_FLOW_PATH = state/ctp/md

INSTRUMENTS = au2612,ag2612
```

敏感项只从环境变量或权限受控文件读取。

---

## 14. 项目结构

```text
quantlink-ors/
  CMakeLists.txt
  README.md
  .gitignore
  third_party/ctp/          # 不提交 SDK，放头文件/so
  docs/
    2026-11-24-l0-ors-ctp-design.md
  ctp/
    CMakeLists.txt
    include/ors/ctp/
      CtpConfig.h
      CtpSession.h
      CtpAdapter.h
      CtpOrderRefMap.h
    src/
      CtpSession.cpp
      CtpTraderSpi.cpp
      CtpMdSpi.cpp
      CtpAdapter.cpp
      CtpOrderRefMap.cpp
      main.cpp
    tests/
    configs/
      simnow.example.cfg
```

---

## 15. 实施阶段

### Phase 1：CTP 连接

```text
CMake + CTP SDK 发现
配置加载
Trader/Md 登录
结算确认
VenueSession 状态
```

验收：SimNow 登录成功，查询 instrument 成功，无订单。

### Phase 2：行情与查询

```text
Md 订阅
20 档补零行情
Account/Position/Order/Trade 查询
```

验收：190 上看到行情流和快照。

### Phase 3：订单

```text
OrderRequest -> CTP ReqOrderInsert
CancelRequest -> ReqOrderAction
OrderUpdate/Trade
```

验收：SimNow 下单/撤单/成交/回报。

### Phase 4：Contract 闭环

```text
C++ SBE codec
Aeron publication
FastTrade/AccessClient 消费
```

验收：L1 消费 ORS 的 0.2.0 消息。

### Phase 5：重连

```text
断线重连
session epoch
position snapshot
订单对账由 L2/LocalAgent/ReadModel 负责
```

验收：断线后 ORS 不重复维护订单状态，L2/LocalAgent/ReadModel 能完成对账。

---

## 16. 验收

```text
1. 190 上 CMake 构建通过
2. SimNow Trader/Md 登录通过
3. instrument 查询通过
4. 行情 20 档（不足补零）通过
5. Account/Position 快照通过
6. 下单/撤单/成交回报通过
7. contract 0.2.0 编码字段正确
8. 重连 epoch/position snapshot 通过；订单对账由 L2/LocalAgent/ReadModel 验收
9. 无密码/AppID/AuthCode 入库
```

---

## 17. 未决

```text
1. 固定 CTP SDK 版本（SimNow 当前支持版本）
2. C++ SBE/Aeron 依赖的构建方式
3. CTP 不支持的 ModifyRequest 错误码
4. SimNow 测试账户/交易时段
5. 是否允许同一 ORS 连接多个 CTP 前置
```
