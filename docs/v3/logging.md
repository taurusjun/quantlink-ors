# v3 L0 ORS 日志与指标规范

- 状态：DRAFT
- 日期：2026-11-26
- 范围：L0 ORS 关键路径日志、关联字段、级别、采样和脱敏

---

## 1. 目标

日志要能回答：

```text
这笔订单从 L1 到 CTP 发了什么？
CTP 返回了什么？
为什么被丢弃/拒绝/重复？
最终发到 contract 的帧是什么？
发生在哪个 session/epoch/sequence？
耗时多少？
```

日志不是“打印变量”，而是可复现运行场景的事实记录。

---

## 2. 技术选择

```text
spdlog async_logger
  rotating_file_sink 100MB x 5
  queue 8192
  overrun_oldest
  后台线程写文件
```

注意：

```text
spdlog async 只把 I/O 放到后台线程；
消息格式化仍在调用线程。
```

因此：

```text
订单/撤单/回报/错误：INFO/WARN/ERROR 结构化日志
行情每 tick：不打印，只计数
连接/查询/重连：INFO
热点内部循环：只 atomic counter
```

不用 `std::cout` / `std::cerr` 作为生产日志。

---

## 3. 日志格式

使用 **JSON Lines**：一行一个 JSON 对象。参考公司 ORS 的字段集合，但不复制 `|`
拼接格式。

示例：

```json
{"ts":"2026-11-26T11:02:49.123456+08:00","level":"INFO","event":"order.rtn","venue_id":"SIMNOW","ors_id":"ors-ctp-1","account_id":"142266","source_session_id":1795661704854903190,"source_publication_epoch":1,"stream_id":9004,"sequence":6,"order_ref":42,"request_id":"req-42","ctp_ref":"000000000042","front_id":3,"session_id":400,"order_sys_id":"424017","instrument_id":"au2612","exchange":"SHFE","side":"BUY","offset_flag":"OPEN","order_type":"LIMIT","time_in_force":"DAY","price":735.0,"qty":1,"filled_qty":1,"remaining_qty":0,"avg_price":735.0,"status":"FILLED","cancel_state":"NONE","error_code":0,"action":"published","latency_us":312}
```

字段：

```text
ts, level, event
venue_id, ors_id, account_id, environment
source_session_id, source_publication_epoch, stream_id, sequence
order_ref, request_id, ctp_ref, front_id, session_id, order_sys_id, trade_id
instrument_id, exchange
side, offset_flag, order_type, time_in_force
price, qty, filled_qty, remaining_qty, avg_price
status, cancel_state, error_code, rc
latency_us
action, drop_reason, reason
```

规则：

```text
数字字段保持 JSON number，不转成字符串
reason/error 必须转义，不含换行控制字符
reason/error 先脱敏再截断
缺失字段不输出，不写 null
每行必须是合法 JSON，不能用原始 | 分隔
```

公司 ORS 字段映射：

```text
ClientID       -> account_id
Symbol         -> instrument_id
localID        -> order_ref
exchID/OrderRef-> ctp_ref
OrderSysID     -> order_sys_id
TradeID        -> trade_id
Response_Type  -> event/status/action
Price/Quantity -> price/qty
openQty        -> remaining_qty
```

---

## 4. 关键打印位置

参考公司 ORS 的打印点，但去掉无结构拼接和重复行。

| 位置 | event | 级别 | 关键字段 |
|---|---|---|---|
| 启动配置 | `ors.start` | INFO | venue/ors/account/environment/instruments/streams/epoch（不含凭据） |
| CTP/MD 连接 | `session.state` | INFO | from/to/reason/front/session/trading_day |
| 断线 | `session.disconnected` | WARN | reason/front/session |
| 认证/登录/结算 | `session.handshake` | INFO | step/rc/error_code/reason |
| 收到 OrderRequest | `order.recv` | INFO | order_ref/request_id/instrument/side/offset/type/tif/price/qty |
| 发 CTP 前 | `order.send` | INFO | order_ref/ctp_ref/instrument/side/offset/price/qty |
| ReqOrderInsert 返回 | `order.send_result` | INFO/WARN | order_ref/ctp_ref/rc |
| OnRspOrderInsert | `order.rsp_insert` | WARN/ERROR | order_ref/ctp_ref/error_code/reason/action |
| OnErrRtnOrderInsert | `order.err_insert` | WARN/ERROR | same |
| OnRtnOrder 原始 | `order.rtn` | INFO | order_ref/ctp_ref/front/session/sys_id/status/cancel_state/filled/remaining/reason/action |
| OnRtnTrade 原始 | `trade.rtn` | INFO | trade_id/order_ref/ctp_ref/sys_id/instrument/price/qty/trading_day |
| 发布 OrderUpdate | `publish.order` | INFO | order_ref/status/cancel_state/sequence/stream_id/result |
| 发布 Trade | `publish.trade` | INFO | trade_id/order_ref/sequence/stream_id/result |
| 发布失败 | `publish.fail` | ERROR | frame_type/stream_id/sequence/error/backpressure_count |
| 撤单收到 | `cancel.recv` | INFO | order_ref/request_id/instrument |
| 撤单发送 | `cancel.send` | INFO | order_ref/ctp_ref/front/session |
| 撤单响应 | `cancel.rsp` | INFO/WARN | order_ref/error_code/reason/result |
| 撤单回报 | `cancel.rtn` | INFO | order_ref/status/cancel_state/reason |
| 未知/重复/旧 front | `report.drop` | WARN | order_ref/ctp_ref/front/session/reason/drop_reason |
| 账户查询 | `query.account` | INFO | request_id/elapsed/rows/error |
| 持仓查询 | `query.position` | INFO | request_id/elapsed/rows/error |
| 重连 | `reconnect.epoch` | INFO | old_epoch/new_epoch/front/session/elapsed |
| MD 首次 tick | `md.first_tick` | INFO | instrument/exchange/last/volume/levels |
| MD stale | `md.stale` | WARN | instrument/last_tick_age_ms |
| 关闭 | `ors.shutdown` | INFO | reason/uptime |

### 4.1 公司 ORS 对应的打印点

```text
SendNewOrder start/end                 -> order.send / order.send_result
OnRspOrderInsert                       -> order.rsp_insert
OnErrRtnOrderInsert                    -> order.err_insert
OnRtnOrder start/end + status branches -> order.rtn + publish.order
OnRtnTrade                             -> trade.rtn + publish.trade
SendCancelOrder                        -> cancel.send
OnRspOrderAction / OnErrRtnOrderAction -> cancel.rsp
Position/Account query                 -> query.position / query.account
FrontDisconnected / login              -> session.disconnected / session.state
```

公司 ORS 的每笔订单/成交日志保留；行情每 tick 不照搬。

---

## 5. 关联与复现

每条订单相关日志必须带同一组键：

```text
order_ref
request_id
ctp_ref
front_id/session_id
order_sys_id
trade_id（成交）
source_session_id/source_publication_epoch
sequence（发布后）
```

这样可以按 `order_ref` 或 `trade_id` 还原：

```text
L2 请求 -> ORS 映射 -> CTP 请求 -> CTP 原始回报
  -> contract OrderUpdate/Trade -> Aeron sequence
```

错误日志必须记录“为什么丢弃/拒绝”：

```text
action=accepted|published|ignored|duplicate|stale|rejected
drop_reason=unknown_order_ref|old_front|terminal|invalid_identity|...
```

---

## 6. 指标

热点路径只计数，不逐条打印：

```text
orders_received
orders_sent
ctp_order_callbacks
ctp_trade_callbacks
aeron_publish_ok
aeron_backpressure
aeron_failed
unknown_reports
stale_front_reports
md_ticks
md_stale
reconnects
query_failures
```

每 60 秒一条 `metrics` 汇总日志；关闭时再打印一次。

---

## 7. 安全

```text
不打印 PASSWORD / APP_ID / AUTH_CODE
不打印完整请求 payload 中的凭据
reason/error 先脱敏再截断
日志文件权限受控
```

---

## 8. 不做

```text
不打印每一条行情 tick
不打印每个 Aeron 心跳
不打印每个 MD 级别
不把日志当日志数据库用于订单恢复
```
