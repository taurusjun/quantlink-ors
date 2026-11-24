# ctp

CTP 子项目：TraderApi + MdApi 对接，输出 L0 contract 0.2.0。

第一版边界：

```text
1 个 ORS = 1 个 CTP 账户
1 个 TraderApi session
1 个 MdApi session
```

不负责：

```text
策略
GUI
账户路由
PnL 计算
```

目录规划：

```text
include/ors/ctp/   公共接口
src/               实现
tests/             单测/集成测试
configs/           示例配置（不含密码）
```

CTP SDK 不要提交到仓库，构建时通过 `CTP_HOME` 或 `third_party/ctp` 提供。
