# v3 ORS 基础库

- 状态：DRAFT
- 日期：2026-11-26
- 范围：`ors_base` 基础组件和 CTP 专用配置层的依赖关系

---

## 1. 目标

把与 CTP 业务无关、但被多个模块使用的组件放到一个基础库：

```text
ors_base
  ├─ ConfigFile
  ├─ Log
  ├─ Metrics
  ├─ ContractText
  └─ StringList
```

CTP 专用内容不进入基础库：

```text
CtpConfig
ContractRuntimeConfig
CtpAdapter
CTP 回调/订单映射
```

---

## 2. 目录结构

```text
ctp/include/ors/base/
  ConfigFile.h
  Log.h
  Metrics.h
  ContractText.h
  StringList.h

ctp/src/base/
  ConfigFile.cpp
  Log.cpp
  Metrics.cpp
  ContractText.cpp
  StringList.cpp

ctp/tests/
  ConfigFileTest.cpp
  LogTest.cpp
  MetricsTest.cpp
  ContractTextTest.cpp
  StringListTest.cpp
```

命名空间：

```cpp
namespace ors::base;
```

CTP 专用代码继续使用：

```cpp
namespace ors::ctp;
```

---

## 3. ConfigFile

使用 **inih** 作为 INI 解析后端，外层保留严格 `ConfigFile` API。

inih 只负责解析 `key=value`、section 和注释；我们不在业务代码直接调用 inih。

格式：

```text
key = value
# 整行注释
; 整行注释
```

规则：

```text
inih 编译定义：
  INI_ALLOW_INLINE_COMMENTS 0
  INI_ALLOW_MULTILINE 0
  INI_ALLOW_NO_VALUE 0
  INI_MAX_LINE 4096

只在行首（允许前导空白）识别 # 或 ; 注释
值中间的 # / ; 保留，避免 PASSWORD 被截断
section 直接拒绝，当前 ORS 使用 flat 配置
重复 key 后写覆盖
缺失/空 required key 抛带 label 的异常
不打印值
```

API：

```cpp
class ConfigFile {
 public:
  static ConfigFile load(const std::string& path, const std::string& label);
  std::string get(const std::string& key) const;
  bool contains(const std::string& key) const;
  std::string require(const std::string& key, const char* envName = nullptr) const;
};
```

inih 源码只放：

```text
third_party/inih/ini.c
third_party/inih/ini.h
```

pin 版本，不依赖系统包。后续如果确实需要 section，再单独设计；v3 不提前实现。

---

## 4. Log

基于 spdlog async logger 的封装，输出 JSON Lines：

```cpp
struct LogField {
  std::string_view key;
  std::variant<std::string, std::int64_t, std::uint64_t, double, bool> value;
};

void logEvent(spdlog::logger& logger,
              spdlog::level::level_enum level,
              std::string_view event,
              std::initializer_list<LogField> fields);
```

约束：

```text
spdlog async + rotating file
queue 8192，overrun_oldest
一行一个合法 JSON
数字字段保持 number
字符串做 JSON 转义
不打印凭据
订单/撤单/成交/错误走结构化事件
行情每 tick 只计数
```

业务代码不直接依赖 spdlog 的格式化宏；通过 `Log.h` 统一。

---

## 5. Metrics

原子计数器，不引入 Prometheus：

```cpp
struct Metrics {
  std::atomic<std::uint64_t> ordersReceived{0};
  std::atomic<std::uint64_t> ordersSent{0};
  std::atomic<std::uint64_t> ctpOrderCallbacks{0};
  std::atomic<std::uint64_t> ctpTradeCallbacks{0};
  std::atomic<std::uint64_t> aeronPublishOk{0};
  std::atomic<std::uint64_t> aeronBackPressure{0};
  std::atomic<std::uint64_t> aeronFailed{0};
  std::atomic<std::uint64_t> unknownReports{0};
  std::atomic<std::uint64_t> staleFrontReports{0};
  std::atomic<std::uint64_t> mdTicks{0};
  std::atomic<std::uint64_t> mdStale{0};
  std::atomic<std::uint64_t> reconnects{0};
  std::atomic<std::uint64_t> queryFailures{0};
};
```

每 60 秒 `logMetrics()` 汇总一次；关闭时再打一次。

---

## 6. ContractText

统一固定宽度字段处理：

```cpp
std::string fitContractField(std::string_view text, std::size_t maxBytes);
void requireContractWidth(std::string_view value,
                          std::size_t maxBytes,
                          const std::string& key);
```

`fitContractField` 必须 UTF-8 安全截断；`requireContractWidth` 在配置加载时校验
`VENUE_ID`/`ORS_ID`/`ENVIRONMENT` 等字段。

---

## 7. 依赖关系

```text
ors_base
  -> spdlog / fmt / Threads

ors_ctp_config
  -> ors_base (ConfigFile, ContractText)
  -> CtpConfig / RuntimeConfig

ors_ctp_core
  -> ors_ctp_config
  -> ors_base (Log, Metrics, ContractText, StringList)

ors_aeron
  -> ors_ctp_core
  -> ors_base
```

禁止：

```text
ors_base -> ors_ctp_core
ors_base -> CTP SDK
ors_base -> Aeron 业务发布
```

---

## 8. 迁移

1. 新建 `ors_base` target，加入 ConfigFile/Log/Metrics/ContractText/StringList。
2. 把现有 `ConfigFile`、`StringList` 从 `ors_ctp_config` 移到基础库，更新 include/namespace。
3. `ors_ctp_config` 链接 `ors_base`，只保留 `CtpConfig`、`RuntimeConfig`。
4. 把 CTP runtime/session 的 `std::cerr` 替换为 `ors::base` 日志。
5. 测试先写，再迁移；每一步保持 ctest 绿色。
