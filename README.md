# quantlink-ors

独立 ORS（Order Routing System）仓库。

当前范围：

```text
L0 / ORS
  └─ ctp    CTP TraderApi / MdApi 对接
```

设计文档：

- [L0 ORS CTP 对接方案](docs/2026-11-24-l0-ors-ctp-design.md)
- [CTP/SimNow 验收记录（PARTIAL）](docs/2026-11-24-ctp-acceptance.md)
- [自包含检查](docs/2026-11-25-self-contained.md)

Contract C++ SBE 生成：

```bash
SBE_JAR=/path/to/sbe-all-1.35.6.jar \
CONTRACT_HOME=/path/to/quantlink-venue-contract \
./tools/gen_contract_cpp.sh
```

生成物写到 `build/generated/contract/`，不提交。

190 构建需要显式提供 CTP/Aeron 路径：

```bash
cmake -S . -B build \
  -DCTP_HOME=/root/quantlink-trading/ors/third_party/ctp/linux \
  -DAERON_HOME=/opt/quantlink-fin/aeron-src \
  -DAERON_RUNTIME_HOME=/opt/quantlink-fin/aeron-runtime \
  -DCONTRACT_GENERATED_DIR="$PWD/build/generated/contract"
cmake --build build -j
ctest --test-dir build --output-on-failure
```

不提供 `AERON_HOME` 或 `AERON_RUNTIME_HOME` 时 CMake 会直接报错，不再默认依赖 `/opt` 路径。

Offset 语义：

```text
CTP ORS 只把 L2 已解析的最终 offsetFlag 1:1 映射到 CTP：
OPEN / CLOSE / CLOSE_TODAY / CLOSE_YESTERDAY。
SHFE/INE 的平今/平昨拆分由 L2 ExecutionOffsetPolicy 负责；
CLOSE 在 SHFE/INE 表示平昨，平今必须显式 CLOSE_TODAY。
非 SHFE/INE 的显式平今/平昨由 L2 拒绝，CTP ORS 不改写、不猜。
```

原则：

```text
146 源码只读
CTP SDK 不提交二进制
SimNow 密码/AppID/AuthCode 不入库
L0 contract 由 quantlink-venue-contract 定义
```

子项目：

```text
ctp/       CTP 连接、订单、回报、行情、账户/持仓查询
```

构建与测试目标：

```text
190 Rocky Linux 9 / x86_64
CMake + C++20
本机只编辑，不在本机跑 C++/Gradle 测试
```
