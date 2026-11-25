# 2026-11-25 CTP ORS 设计改动

- 状态：DRAFT
- 范围：`quantlink-ors` CTP 分支
- 当前市场：SHFE
- 业务基准：
  - TB：`tb_origin = /Volumes/DevDisk/git/tb_origin/tbsrc`
  - ORS：`/Users/wangjun/Documents/my/fin/fromCompany/gitrepo/ors/China`
- 已定决策：
  - CTP 持仓查询：保留
  - MARKET：允许
  - STP：不做；STP 在 L2，默认关闭
  - 不拆单；L0 只做最终 `offsetFlag` 1:1 映射

---

## 1. 职责边界

CTP ORS 负责：

```text
CTP 登录/会话
OrderRequest -> CTP InputOrderField 映射
订单插入/撤单
OnRtnOrder / OnRtnTrade -> OrderUpdate / Trade
CTP 持仓查询 -> PositionSnapshot
重连/对账
```

不负责：

```text
开平推断（AUTO）
平仓预扣/释放
拆单
STP
RMS
策略归属
```

L0 只接收 L2 已经解析好的最终 `offsetFlag`。

---

## 2. TIF / 报单类型

当前只做 SHFE，允许：

```text
DAY / FAK / IOC / FOK / MARKET
```

### 2.1 映射

```text
DAY:
  GFD + AV + MinVolume=1

FAK:
  IOC + AV + MinVolume=1

IOC:
  IOC + CV + MinVolume=qty

FOK:
  IOC + CV + MinVolume=qty
```

限价：

```text
OrderPriceType = LimitPrice
LimitPrice     = price
```

MARKET：

```text
MARKET + FAK:
  AnyPrice + IOC + AV + MinVolume=1

MARKET + IOC/FOK:
  AnyPrice + IOC + CV + MinVolume=qty

MARKET + DAY:
  LimitPrice + LimitPrice=0 + GFD + AV + MinVolume=1
```

说明：

- `MARKET + DAY` 是公司 China ORS 原映射；需在 CTP 前置验证。
- `tb_origin` 只发 LIMIT + DAY/FAK；MARKET 只影响手工/API。
- 当前不实现 CFFEX/ZCE 的 FOK/IOC/FAK 本地拒绝；后续接入再评估。

---

## 3. 订单生命周期

### 3.1 新单

```text
OrderRequest
  -> validate
  -> ReqOrderInsert
  -> OrderUpdate(NEW)（可选，内部状态）
  -> OnRtnOrder Accepted/NoTradeQueueing -> ACK
  -> OnRtnOrder PartTraded -> PARTIAL
  -> OnRtnTrade -> Trade
  -> 全部成交 -> FILLED
```

### 3.2 拒单

```text
OnRspOrderInsert / OnErrRtnOrderInsert
  -> 对 NEWORDER 一律 REJECTED
  -> 不因 ErrorID==0 忽略
  -> 终态
```

### 3.3 撤单

```text
CancelRequest
  -> ReqOrderAction
  -> 成功发送可内部记 REQUESTED
  -> OnRtnOrder Canceled -> CANCELED
  -> OnErrRtnOrderAction -> cancelState=REJECTED
```

设计约束：

```text
ACK 只发一次
FILLED 不早于 Trade
Trade 更新 filledQty/remainingQty
终态从 active map 移除或归档
```

---

## 4. 持仓快照

启动和重连时查询 CTP 持仓，发布 `PositionSnapshot`。

昨仓：

```text
yesterday = max(0, Position - TodayPosition)
```

不使用 `YdPosition`。

原因：

```text
YdPosition 是昨日结转值，平仓后不减少；
用它会把昨仓可平量算大。
```

约束：

```text
查询完成前不进入 READY
查询失败不能静默使用旧快照
快照带 snapshotTimestamp / staleAfterMs
L2 用快照对账预扣账本
```

CTP 查询限流：

```text
启动/重连重试
查询失败降级或拒绝新单
```

---

## 5. 平昨 / 平今

SHFE：

```text
CLOSE_TODAY -> THOST_FTDC_OF_CloseToday
CLOSE       -> THOST_FTDC_OF_Close
```

CTP 不使用 `THOST_FTDC_OF_CloseYesterday`。

如果 contract 允许 `CLOSE_YESTERDAY`：

```text
CTP adapter 映射为 Close
或 L2 对 SHFE 昨仓直接返回 CLOSE
```

---

## 6. 回报映射

```text
OnRtnOrder:
  Accepted / NoTradeQueueing -> ACK
  PartTradedQueueing         -> PARTIAL
  AllTraded                  -> 等 Trade，不提前 FILLED
  Canceled                   -> CANCELED
  InsertRejected             -> REJECTED

OnRtnTrade:
  Trade
  更新 filledQty / remainingQty
```

未知 `orderRef`：

```text
进入 pending/reconciliation
不按价格/数量猜订单
不静默丢弃
```

---

## 7. MARKET

MARKET 允许，映射按第 2 节。

不因 SHFE 拒绝 MARKET。

MARKET 的最终语义由 CTP 前置/交易所决定；新 ORS 不做额外本地拒绝。

---

## 8. 与 L2 的接口

L0 接收：

```text
orderRef
instrumentId / exchange
side
offsetFlag（最终）
orderType
timeInForce
price / qty
accountId（venue 账户）
requestId
sourceSessionId / sourcePublicationEpoch
```

L0 发布：

```text
OrderUpdate
Trade
PositionSnapshot
AccountSnapshot
VenueSession
```

L0 不改写：

```text
offsetFlag
qty
orderRef
```

---

## 9. 不做

```text
不做 AUTO offset
不做平仓预扣/释放
不做拆单
不做 STP
不做 RMS
不把 strategyId 写进 CTP 协议
不用 position.csv 作为账户真相
```

---

## 10. 验收

```text
1. FAK -> IOC + AV + MinVolume=1
2. DAY -> GFD + AV + MinVolume=1
3. IOC/FOK -> IOC + CV + MinVolume=qty
4. MARKET 映射符合公司 ORS
5. OnRspOrderInsert ErrorID=0 -> REJECTED
6. Trade 先于 FILLED，filledQty/remainingQty 正确
7. 终态订单清理
8. 昨仓 = Position - TodayPosition
9. SHFE 平昨 wire flag = Close
10. 查询完成前不 READY
```

---

## 11. 参考

```text
quantlink-fasttrade/docs/v2/005-l2-trading-channel.md
quantlink-fasttrade/docs/v2/006-l3-runtime-registration.md
quantlink-fasttrade/docs/v2/ref/001-tb-origin-parity.md
quantlink-fasttrade/docs/v2/ref/002-china-ctp-ors-parity.md
```
