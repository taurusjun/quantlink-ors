# L0 ORS 设计（CTP）

- 状态：DRAFT（2026-11-26）
- 范围：L0 ORS / Venue，CTP TraderApi + MdApi
- 上游：L2 TradingChannel / L1 COMMAND
- 下游：L1 EXECUTION / STATE / SESSION / MARKET_DATA
- 关联：
  - `quantlink-fasttrade/docs/v3/001-layer-boundaries.md`
  - `quantlink-fasttrade/docs/v3/ref/001-legacy-mapping.md`
  - `quantlink-venue-contract/docs/contract-fields.md`

---

## 1. 定位

L0 ORS 是 **venue 访问运行时**：

```text
一个 ORS 实例
  = 一个 CTP 交易账户
  + 一个 TraderApi 会话
  + 一个 MdApi 会话
  + contract 0.2.0 / Aeron 发布
```

负责：

```text
CTP 登录/认证/结算确认
instrument 查询
行情订阅
下单/撤单
OrderUpdate / Trade 回报
AccountSnapshot / PositionSnapshot 发布
venue market data 接入
orderRef -> CTP 路由地址的 live 上下文
```

不负责：

```text
strategyId / 策略路由
订单记录/订单查询
订单状态权威
RMS
AUTO 开平
平仓预扣
账户/持仓真相
GUI / 读模型
```

L0 不查询 CTP 订单/成交。订单状态由 L2 `OrderRegistry` 拥有；订单读模型由
L3 FactPublisher / L4 LocalAgent / L5 ReadModel 生成。

L0 的 `OrderState` 是 live 路由上下文，不是订单账本：

```text
orderRef -> CTP FrontID / SessionID / OrderRef / OrderSysID
撤单寻址
迟到/重复回报识别
```

终态事件发布后立即删除，不保留 tombstone。
`orderRef` 由 L2 分配；L0 不生成、不假设、不复用。

---

## 2. 组件

```text
ctp/src/main.cpp
  --config PATH -> CtpConfig + ContractRuntimeConfig -> ContractRuntime

CtpTraderSession
  TraderApi、登录/结算/查询、SPI 转发
  对外暴露 state/tradingDay/frontId/sessionId/traderApi

CtpMdSession
  MdApi、行情订阅、最新 tick

CtpAdapter
  OrderRequest/CancelRequest -> CTP
  OnRsp/OnRtn/OnErr -> OrderUpdate/Trade
  live OrderRef/OrderState 管理
  终态删除、CancelRejected 去重、旧 front 过滤

CtpQueryService
  account/position 查询，发布快照
  order/trade 查询只供 query-smoke 诊断，runtime 不调用

ContractSink
  contract 0.2.0 SBE 编码
  Aeron IPC publish

AeronCommandSubscriber
  order stream / cancel stream -> CtpAdapter

ContractRuntime
  长驻循环：session 状态、命令、行情、快照、epoch
```

Smoke 工具在独立二进制：

```text
ors_ctp_smoke
  login-smoke / md-smoke / query-smoke / order-smoke
  cancel-reject-smoke / lifecycle-smoke
```

生产 `ors_ctp` 只接受 `--config PATH` 和 `--help`，不含 smoke 逻辑。

---

## 3. 配置

配置是同一个 `key=value` 文件，`CtpConfig` 和 `ContractRuntimeConfig` 共享 `ors_base::ConfigFile` 严格解析。

### 3.1 CTP 连接

```text
BROKER_ID
INVESTOR_ID
PASSWORD            可用 CTP_PASSWORD 覆盖
APP_ID              可用 CTP_APP_ID 覆盖
AUTH_CODE           可用 CTP_AUTH_CODE 覆盖
TRADE_FRONT         或 FRONT_ADDR_ORS
MD_FRONT            或 FRONT_ADDR_MD
TRADER_FLOW_PATH
MD_FLOW_PATH
```

### 3.2 L0 runtime

```text
AERON_DIR
VENUE_ID
ORS_ID
ENVIRONMENT
EXCHANGE
INSTRUMENTS
STREAM_BASE
SOURCE_PUBLICATION_EPOCH
```

可选：

```text
SOURCE_SESSION_ID        0/省略 = runtime 启动生成
QUERY_INTERVAL_SECONDS   默认 1
```

### 3.3 Stream 布局

```text
STREAM_BASE = 9000

base+1 market data
base+2 order
base+3 cancel
base+4 execution
base+5 position
base+6 account
base+7 session
```

### 3.4 固定宽度

contract 的字符串字段是固定宽度。映射时必须：

```text
CTP 字段读取按数组长度截断
OrderSysID / TradeID trim 前后空格
reason 按 contract 字段 UTF-8 安全截断
VENUE_ID / ORS_ID / ENVIRONMENT 配置校验
```

任何编码失败不能让异常穿过 CTP 回调线程。

---

## 4. 启动与会话

启动顺序：

```text
CONNECTING
  -> AUTHENTICATING
  -> LOGGING_IN
  -> SETTLEMENT_CONFIRMING
  -> QUERYING_INSTRUMENTS
  -> READY
```

`READY` 只表示 CTP TraderApi 会话可用。
account/position 查询失败不改变 `READY`；快照失败由 L2/AccountSnapshotService 按
`staleAfterMs` 降级。

MD 独立：

```text
MD CONNECTING -> LOGGING_IN -> READY
```

MD 断线不能让 Trader session 变成不可用，但必须发布 `VenueSession` 降级或停止
行情发布，不能把旧 tick 当新行情。

---

## 5. 订单

### 5.1 OrderRequest

L2 分配 `orderRef`，L0 消费：

```text
orderRef -> CTP OrderRef
instrumentId / exchange / side / offsetFlag
orderType / timeInForce / price / qty
requestId
```

L0 只做最终 `offsetFlag` 的 1:1 CTP 映射：

```text
OPEN -> Open
CLOSE -> Close
CLOSE_TODAY -> CloseToday
CLOSE_YESTERDAY -> Close
```

不拆单、不改写、不判断交易所今昨仓；拆单和 AUTO 由 L2 负责。

### 5.2 发送

```text
1. 注册/校验 orderRef
2. 先写入 live OrderState
3. ReqOrderInsert
4. 发送成功 -> 发布 NEW
5. 发送失败 -> 发布 REJECTED，删除 live 状态
```

先写 state 再发送，避免极快 Trade 回调找不到订单状态。

### 5.3 撤单

`ReqOrderAction` 需要：

```text
FrontID / SessionID / OrderRef / InstrumentID / ExchangeID
```

只从 live state 取。状态未 ACK、已终态、未知 orderRef 一律本地拒绝。
终态订单没有 live 撤单路由。

### 5.4 终态

```text
InsertRejected -> REJECTED
Canceled       -> CANCELED
成交累计量 == orderQty -> FILLED
```

发布终态 OrderUpdate（以及 Trade）后：

```text
orders_.erase(orderRef)
refs_.erase(orderRef)
```

不保留 tombstone；不把终端状态留到下一交易日。
`orderRef` 唯一性由 L2 保证，L0 不讨论复用。

---

## 6. 回报状态机

```text
NEW
  -> ACK
  -> PARTIAL
  -> FILLED
  -> CANCELED
  -> REJECTED
```

约束：

```text
NEW 由发送成功发布
ACK 只发一次
AllTraded 不直接发布 FILLED；等 OnRtnTrade 累计量
Trade 先于 FILLED OrderUpdate 发布
CancelRejected 不把订单置终态
同一撤单请求的 OnRspOrderAction + OnErrRtnOrderAction 只发布一次 REJECTED
未知 orderRef 的回报直接丢弃并记录 lastError
```

`CancelRejected` 时：

```text
status 保持 ACK/PARTIAL
cancelState=REJECTED
允许再次撤单
```

---

## 7. Account / Position 快照

L0 必须提供 venue 查询并发布快照：

```text
queryAccount -> AccountSnapshot
queryPositions -> PositionSnapshot
```

来源：

```text
CTP ReqQryTradingAccount / ReqQryInvestorPositionDetail
```

关系：

```text
L0 快照 = venue fact
L2 PositionSnapshot = AUTO/预扣的 venue 基线
AccountSnapshotService = 账户读模型
L0 不是账户/持仓真相
```

`QUERY_INTERVAL_SECONDS` 控制周期。CTP 有查询频率限制；配置不能小于安全间隔。
查询失败：

```text
不改变 Trader READY
不把交易通道判死
记录 error，下个周期重试
```

快照必带：

```text
snapshotId
snapshotTimestamp
staleAfterMs
tradingDay
accountId
```

`CtpQueryService` 的 order/trade 查询只给 `query-smoke` 诊断，不进入 runtime，
不更新 live 订单状态。

---

## 8. 行情

```text
SubscribeMarketData
OnRtnDepthMarketData
```

映射：

```text
CTP 5 档 -> contract 20 档
不足补 price=0 / qty=0
不重复、不伪造
```

行情是独立 MD 链路；ORS 不拥有行情事实。L2/L3 按 `ContractKey` 和 sequence 消费。

MD 断线必须：

```text
标记 DOWN / stale
停止把 lastMarketData 当新行情重复发布
重新登录后重新订阅
```

不能用旧 tick 生成新的 sequence 当作实时行情。

---

## 9. 重连 / epoch

```text
Disconnected/Failed after READY
  -> currentEpoch++
  -> VenueSession LOST
  -> re-auth/login/settlement
  -> READY
```

规则：

```text
sourceSessionId 标识 runtime 启动
sourcePublicationEpoch 每次 publication 重建递增
旧 epoch 的 OrderUpdate/Trade 不能当成新事件
```

`CtpAdapter` 用登录返回的 `FrontID/SessionID` 过滤旧 front 回报；
`OnRtnTrade` 用订单绑定的 front/session 过滤。

剩余风险：

```text
如果新旧登录复用相同 FrontID/SessionID，这层无法区分；
需要 CTP 连接代次或更严格的回调隔离。
```

重连不查询订单/成交，不做 ORS 内 reconciliation gate。
account/position 查询周期在重新 READY 后恢复；失败不影响 READY。

---

## 10. 传输与失败

### 10.1 Aeron

执行流使用 `aeron:ipc`，无 Archive/replay。
`offer` 失败不能静默丢：

```text
retry/idle 到期限
记录 back-pressure/not-connected 计数
持续失败 -> 停止接受新单或让 runtime 降级
```

不能因为 Aeron 暂不可用而继续接受订单并丢掉成交。

### 10.2 CTP session

```text
Failed = 需要 supervisor 重启或显式恢复
不能永久停在 Failed 还假装 READY
```

### 10.3 日志

严禁打印：

```text
PASSWORD / APP_ID / AUTH_CODE
```

错误消息可以打印 key 名，不能打印值。

---

## 11. 测试与验收

离线：

```text
ctp_config
ctp_runtime_config
ctp_adapter
ctp_query
ctp_order_ref_map
ctp_contract_sink
ctp_smoke_lifecycle
```

Live：

```text
login-smoke
md-smoke
query-smoke
order-smoke
cancel-reject-smoke
lifecycle-smoke
```

Live lifecycle 必须一次登录执行：

```text
cancel-reject case -> fill case -> account/position query
```

不能每个 case 重新登录，避免 SimNow 会话压力。

---

## 12. 当前实现待补

以下属于 L0 生产化缺口，不改变本文边界：

```text
1. ContractSink reason 超固定宽度需 UTF-8 安全截断
2. Aeron offer 失败需重试/降级/计数
3. MD 断线重登、旧 tick 去重、时间戳
4. SessionState::Failed 恢复或非零退出
5. accountId/venueId/environment 入站校验
6. account/position 查询限流
7. 旧 front/session 策略的严格测试
```

这些修完之前，L0 不宣称 production ready。
