# CTP ORS Self-contained Check

- 状态：PASS（生产源文件）
- 日期：2026-11-25
- 环境：190 Rocky Linux 9

## 结果

```text
repo 内没有 #include 其他项目源码
没有依赖 ctp-adapter / quantlink-fin-src
PriceSanitize 已统一
AERON_HOME 和 AERON_RUNTIME_HOME 必须显式提供
190 clean build + ctest 13/13 PASS
```

## 外部依赖（明确保留）

```text
CTP SDK
  /root/quantlink-trading/ors/third_party/ctp/linux

Aeron
  /opt/quantlink-fin/aeron-src       AERON_HOME 头文件
  /opt/quantlink-fin/aeron-runtime   AERON_RUNTIME_HOME 动态库
```

这些是第三方 SDK/库，不提交二进制或头文件。

## 未迁入的旧 ctp-adapter 文件

```text
sim_exchange.h       旧模拟交易所，测试工具
u50_replay.h         旧 U50 回放工具
fee_refresh_main.cpp 旧费用刷新工具
ctp_aeron_bridge.h   已被 ContractSink + AeronFramePublisher 替代
```

生产 CTP ORS 不需要它们。若以后要做本地模拟/回放验收，作为独立 test tool 再加入。

## 构建验证

```bash
SBE_JAR=/root/sbe-all-1.35.6.jar \
CONTRACT_HOME=/opt/quantlink-venue-contract \
./tools/gen_contract_cpp.sh

cmake -S . -B build \
  -DCTP_HOME=/root/quantlink-trading/ors/third_party/ctp/linux \
  -DAERON_HOME=/opt/quantlink-fin/aeron-src \
  -DAERON_RUNTIME_HOME=/opt/quantlink-fin/aeron-runtime \
  -DCONTRACT_GENERATED_DIR="$PWD/build/generated/contract"
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Result:

```text
100% tests passed, 0 tests failed out of 13
```

Missing `AERON_HOME` or `AERON_RUNTIME_HOME` fails fast:

```text
CMake Error at ctp/CMakeLists.txt:17 (message):
  AERON_HOME and AERON_RUNTIME_HOME are required
```
