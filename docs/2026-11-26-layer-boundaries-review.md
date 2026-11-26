# 2026-11-26 新系统 L0-L6 边界与依赖复盘

- 状态：评审记录
- 范围：L0 ORS、L1 IPC、L2 TradingChannel、L3 TradeBot、L4 LocalAgent、L5 BotGateway、L6 GUI，以及 AccountSnapshotService / LocalAgent / ReadModel 的旁路位置
- 关联：
  - `quantlink-fasttrade/docs/v2/002-system-layers.md`
  - `quantlink-fasttrade/docs/v2/003-l0-ors-venue.md`
  - `quantlink-fasttrade/docs/v2/004-l1-ipc-transport.md`
  - `quantlink-fasttrade/docs/v2/005-l2-trading-channel.md`
  - `quantlink-fasttrade/docs/v2/006-l3-runtime-registration.md`

---

## 1. 结论

这次讨论里，至少有四件事需要分开：

```text
1. 公司 ORS 是 L2+L0 单体，不是新系统 L0 的基线
2. 很多业务功能确实搬到了 L2，但 account/position 查询不能一起搬走
3. v2 文档里有几处职责冲突，必须先把它们定死
4. 当前 ORS 已按 L0 收缩，但仍有 L0 生产可靠性缺口
```

一句话边界：

```text
L0 只提供 venue 访问和 venue 事实
L1 只负责传输、序列、路由，不建立订单状态
L2 拥有通道订单状态、orderRef、预扣和 AUTO/平仓策略
L3 拥有策略意图和策略级归属
AccountSnapshotService / LocalAgent / ReadModel 是读模型和账户旁路，不进入订单主链路
```

---

## 2. 各层职责

### L0 ORS / Venue

负责：

```text
CTP TraderApi / MdApi 会话
登录/认证/结算确认
下单/撤单
OnRtnOrder / OnRtnTrade 映射
venue account/position 查询并发布 AccountSnapshot / PositionSnapshot
venue 行情接入
```

不负责：

```text
strategyId
订单记录/订单查询
订单状态权威
RMS
AUTO 开平
平仓预扣
订单对账
账户/持仓真相
```

L0 的 `orders_` 只能是与 CTP 回调之间的 live 路由上下文：

```text
orderRef -> FrontID/SessionID/OrderRef/OrderSysID
撤单寻址
迟到/重复回报识别
终态后立即从 live map 删除
```

L0 的 PositionSnapshot / AccountSnapshot 是 **venue fact**，不是账户真相。
L2/AccountSnapshotService 负责解释、对账和持有真相。

### L1 IPC / Transport

负责：

```text
COMMAND / EXECUTION / STATE / SESSION / MARKET_DATA
sequence、gap 标记
account 级 execution 流路由到对应 TradingChannel
PositionSnapshot 路由到对应 TradingChannel
AccountSnapshot 路由到 AccountSnapshotService
```

不负责：

```text
订单状态
策略归属
订单对账
读模型
```

L1 不建立订单状态，未知 orderRef 不补状态。

### L2 TradingChannel

负责：

```text
单 exchange 通道
orderRef 分配
OrderRegistry
FillLedger
channelEpoch
AUTO 开平、平仓预扣/释放
订单/成交回报归属
PositionSnapshot 到预扣账本的基线更新
可选 STP（默认关闭）
```

不负责：

```text
策略决策
策略级持仓归属
RMS（待定，见第 4 节）
GUI
读模型
```

L2 是通道订单状态的唯一写入者。

### L3 TradeBot

负责：

```text
StrategyRuntime
TradeBotOrderGateway
OrderIntent
策略级订单/仓位视图
MarketDataRouter
FactPublisher
```

L3 不写通道订单状态；它只通过 L2 发送 OrderIntent。

### L4 LocalAgent

负责：

```text
采集 Fact
本地缓冲/合并
转发给 BotGateway
```

不进入订单主链路。

### L5 BotGateway

负责：

```text
认证/授权
读模型查询/订阅
控制命令入口
手工单入口
ACK/审计
```

不负责订单状态权威、成交归属、PnL。

### L6 GUI

只读展示和操作入口。

---

## 3. 依赖方向

```text
L6 GUI
  -> L5 BotGateway
    -> L4 LocalAgent / 控制命令
      -> L3 StrategyRuntime

L3 StrategyRuntime
  -> TradeBotOrderGateway
    -> L2 TradingChannel
      -> L1 IPC
        -> L0 ORS
          -> CTP

L0 ORS
  -> L1 EXECUTION / STATE / SESSION / MARKET_DATA
    -> L2 TradingChannel
    -> AccountSnapshotService
    -> L3 MarketDataRouter
```

允许的旁路：

```text
L0 PositionSnapshot -> L2 TradingChannel（AUTO 预扣基线）
L0 AccountSnapshot  -> AccountSnapshotService（账户读模型）
L0 MarketData       -> L3 MarketDataRouter（不经过 ORS 请求链路）
```

禁止：

```text
L3/L2 -> CTP（绕过 L0）
L2 -> AccountSnapshotService（读模型反写交易）
L0 -> L2 业务状态
L5/GUI -> L0
L0 持有 RMS/订单权威
```

---

## 4. v2 文档里的四个冲突

### 4.1 OrderRegistry / FillLedger 到底在 L2 还是 L3

`002-system-layers.md` 的 L3 图里列了：

```text
OrderRegistry / FillLedger
```

但同一文档的 L2 TradingChannel 章节又说 L2 负责：

```text
OrderRegistry
FillLedger
```

`005-l2-trading-channel.md` 明确写 L2 是订单业务状态拥有者。

结论：**L2 拥有 OrderRegistry / FillLedger**。L3 只能有策略级订单视图，建议改名
`StrategyOrderView`，不要复用 `OrderRegistry` 这个名字。

### 4.2 RMS 放哪

`005` 开头写 L2 不负责 RMS；`005` 末尾又列未决：

```text
RMS 放 L2 还是独立组件
```

`002` 也没有明确 RMS 所属层。

结论：这是**未定义**，不能假装已经定了。可选方案：

```text
A. L2 TradingChannel 内部同步 pre-trade risk gate（简单，但 L2 变重）
B. 独立 RiskService，L2 在下单前同步调用（边界清晰）
C. L3 TradeBotOrderGateway 做策略级风控（只覆盖策略，不覆盖手工单）
```

公司 ORS 的 RMS 不能直接塞回 L0。建议先做 B，或者明确 A 并把文档改一致。

### 4.3 AccountSnapshot 是否给 L2

`002` 禁止：

```text
AccountSnapshotService -> TradingChannel
```

但 `005` 的 L2 输入又列了：

```text
AccountSnapshot
```

`004` 规定：

```text
PositionSnapshot -> TradingChannel
AccountSnapshot  -> AccountSnapshotService
```

结论：**L2 只应消费 PositionSnapshot 作为 AUTO 预扣基线；AccountSnapshot 只进
AccountSnapshotService**。如果 L2 需要保证金/购买力，应通过独立的 risk/account
接口，不要让 AccountSnapshotService 反写交易。

### 4.4 历史 DataService 与 v3 LocalAgent/ReadModel

v2 总览没有 DataService；v3 也不引入 DataService。公司/tb_origin 的
`ServiceOrderDataStore` 只是历史订单读模型参考，不是运行时组件。

v3 的读模型路径是：

```text
L0 OrderUpdate/Trade -> L2/L3 Fact
  -> L4 LocalAgent
    -> L5 ReadModel / BotGateway
```

不能让 L2 依赖历史 DataService 的订单记录做路由或恢复权威。

---

## 5. 与公司 ORS + tb_origin 的职责映射

必须先拆开两个参考系统：

```text
公司 ORS      = CTP 网关 + 订单 map + RMS + SHM
tb_origin     = TradeBot 策略框架 + Connector + 历史 DataService + RMS_NEW
```

它们不是同一个“公司系统”，新系统也不能整体照搬任何一个。

| 公司 ORS / tb_origin 组件 | 实际职责 | 新系统归属 | 是否进 L0 |
|---|---|---|---|
| 公司 ORS `ORSServer` | CTP 会话、下单/撤单、回报 | L0 ORS | 是 |
| 公司 ORS `m_ordLocalMap` / `m_ordExchMap` | live 订单路由 | L0 临时 map | 是，终态即删 |
| 公司 ORS RMS | 撤单计数/预交易检查 | L2/RiskService | 否 |
| 公司 ORS `RemoveFromMap` | 终态/拒单清理 | L0 live routing | 是 |
| tb_origin `Strategies/*` | 策略决策/策略订单视图 | L3 | 否 |
| tb_origin `Connector::GetUniqueOrderNumber` | orderRef/OrderID 分配 | L2 TradingChannel | 否 |
| tb_origin `Connector::HandleOrderResponse` | 回报按 OrderID 归属 | L1/L2 execution routing | 否 |
| tb_origin `DataService/ServiceOrderDataStore` | open/closed 订单读模型 | L4 LocalAgent + L5 ReadModel（历史参考） | 否 |
| tb_origin `NetPnlStore` / PnL | PnL 读模型 | AccountSnapshotService/ReadModel | 否 |
| tb_origin `RMS_NEW` | 风控 | RiskService/L2 | 否 |
| tb_origin `hftbase/Ipc` RequestMsg/ResponseMsg | SHM 传输 | L1 contract/Aeron | 否 |
| tb_origin `ExchangeSimulator` | 模拟交易所 | simvenue/L1 测试 | 否 |

关键结论：

```text
公司 ORS 的订单 map 对应 L0 临时路由，不是 L2 OrderRegistry
tb_origin Connector 的 OrderID 分配对应 L2 orderRef 分配，不是 ORS
tb_origin 历史 DataService 的 open/closed order 是读模型，不是订单权威
tb_origin RMS_NEW / 公司 ORS RMS 都不能塞进 L0
```

因此：

```text
直接使用公司 ORS
  -> 把 L2 订单状态/RMS 重新塞进 L0

直接使用 tb_origin Connector
  -> 保留 SHM 和多层订单 ID 语义，绕过 L1/L2

把历史 DataService 当读模型使用
  -> 把读模型当订单权威
```

三者都不能整体照搬，只能按职责拆开。

---

## 6. 当前 ORS 对照

已符合 L0 的部分：

```text
CTP session / order / cancel / trade 映射
contract 0.2.0 + Aeron
config 驱动身份和 stream
orderRef 分配归 L2；ORS 只消费
订单查询/对账已移出 runtime
终态后删除 live order map
unknown report 不再进入 pending buffer
```

仍需补的 L0 问题：

```text
1. CTP reason 超 contract 固定宽度会抛异常
2. Aeron offer 失败静默丢执行帧
3. MD 断线不重登、旧 tick 重复发布
4. SessionState::Failed 永久卡死
5. 入站 accountId/venueId/environment 未校验
6. account/position 查询频率与 CTP 限流
7. 旧 front 回报策略仍不完整
8. ContractRuntime 不可注入，重连逻辑缺测试
```

特别注意：account/position 查询必须留在 L0 作为 **venue 查询和快照发布能力**，
不能因为“L2 拥有持仓真相”就删掉。L2 没有 CTP session，自己无法查 CTP 持仓。
正确关系是：

```text
L0 查询 CTP -> 发布 PositionSnapshot
L2 消费快照，维护预扣和可用量
AccountSnapshotService 维护账户读模型
```

---

## 6. 需要拍板的事项

```text
1. RMS：L2 内部 / 独立 RiskService / L3（建议独立 RiskService 或 L2）
2. L3 的订单视图是否改名 StrategyOrderView，避免和 L2 OrderRegistry 重名
3. AccountSnapshot 是否只进 AccountSnapshotService；L2 是否只读 PositionSnapshot
4. 历史 DataService 是否只作为参考，v3 读模型走 LocalAgent/ReadModel
5. L0 的 account/position 查询周期和降级策略
```

---

## 7. 最终边界

```text
L0：venue 访问 + venue facts
L1：transport + sequence + routing
L2：通道订单状态 + orderRef + 预扣 + AUTO + 对账
L3：策略意图 + 策略归属
L4：facts 采集/转发
L5：读模型/控制入口
L6：展示
旁路：AccountSnapshotService、LocalAgent、ReadModel 只做读模型/事实转发，不反写交易
```

公司 ORS 只能作为 CTP 行为参考，不能作为新系统基线；直接使用它会把 RMS、订单状态、
订单记录重新塞回 L0，破坏上述边界。
