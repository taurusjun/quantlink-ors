# v3 文档（当前）

- 状态：DRAFT
- 内容：当前 L0 ORS 设计
- 上层架构：`quantlink-fasttrade/docs/v3/001-layer-boundaries.md`
- 参考映射：`quantlink-fasttrade/docs/v3/ref/001-legacy-mapping.md`

## 文档

- [L0 ORS 设计（CTP）](l0-ors-design.md)

## 当前边界

```text
L0 只做 venue 访问和 venue facts
不查订单/成交，不持有订单状态
保留 account/position 查询并发布快照
终态删除 live orderRef 映射
```

历史文档在 `../v1/`、`../v2/`，只作参考。
