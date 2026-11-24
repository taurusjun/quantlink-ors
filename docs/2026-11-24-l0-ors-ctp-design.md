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
7. 断线重连后靠查询/回报对账，不引入历史数据库
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
              ├─ CtpReconciler
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
重连/对账
```

不负责：

```text
策略
账户路由
GUI
PnL 计算
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
  -> 查询 open orders/trades/positions 对账
```

不猜丢失回报。

---

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
5. 重连后按 OrderRef/FrontID/SessionID 查询对账
```

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
ReqQryOrder / ReqQryTrade（对账用）
```

发布：

```text
AccountSnapshot
PositionSnapshot
```

规则：

```text
accountId = CTP InvestorID / ORS venue account
snapshotId / snapshotTimestamp / staleAfterMs 必须完整
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

未知 orderRef 不丢弃：进入 reconciliation buffer，等待查询或后续回报。

---

## 11. 重连与对账

第一版不使用历史数据库，依靠：

```text
OnRtnOrder / OnRtnTrade
ReqQryOrder
ReqQryTrade
ReqQryInvestorPosition
ReqQryTradingAccount
```

重连流程：

```text
1. 清空本 session 的临时 order 映射
2. 登录/结算确认
3. 查询 order/trade
4. 按 OrderRef/FrontID/SessionID 重建
5. 对不上 -> 标 UNKNOWN，禁止新单或降级
6. 发布新的 VenueSession/epoch
```

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
VENUE = CTP

BROKER_ID = 9999
INVESTOR_ID = <simnow-investor>
PASSWORD = <from-env>
APP_ID = simnow_client_test
AUTH_CODE = 0000000000000000

TRADE_FRONT = tcp://...
MD_FRONT = tcp://...

TRADER_FLOW_PATH = state/ctp/trader
MD_FLOW_PATH = state/ctp/md

INSTRUMENTS = ...
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
      CtpReconciler.h
    src/
      CtpSession.cpp
      CtpTraderSpi.cpp
      CtpMdSpi.cpp
      CtpAdapter.cpp
      CtpOrderRefMap.cpp
      CtpReconciler.cpp
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

### Phase 5：重连/对账

```text
断线重连
session epoch
查询对账
UNKNOWN 处理
```

验收：断线后不重复下单、不丢终态。

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
8. 重连后对账通过
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
6. 订单查询频率和快照周期
```
