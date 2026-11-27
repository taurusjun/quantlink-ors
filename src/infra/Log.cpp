#include "ors/base/Log.h"

#include <cctype>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <mutex>
#include <pthread.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>

#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include "ors/base/ContractText.h"
#include "ors/base/Metrics.h"

namespace ors::base {
namespace {

std::string jsonEscape(std::string_view value) {
  std::string out;
  out.reserve(value.size() + 8);
  for (const unsigned char c : value) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (c < 0x20) {
          std::ostringstream escaped;
          escaped << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                  << static_cast<int>(c);
          out += escaped.str();
        } else {
          out.push_back(static_cast<char>(c));
        }
        break;
    }
  }
  return out;
}

std::string lower(std::string_view value) {
  std::string out;
  out.reserve(value.size());
  for (const unsigned char c : value) {
    out.push_back(static_cast<char>(std::tolower(c)));
  }
  return out;
}

bool sensitive(const std::string& key) {
  const std::string name = lower(key);
  return name.find("password") != std::string::npos ||
         name.find("auth_code") != std::string::npos ||
         name.find("app_id") != std::string::npos ||
         name.find("secret") != std::string::npos;
}

std::string eventType(std::string_view event) {
  const auto starts = [event](std::string_view prefix) {
    return event.rfind(prefix, 0) == 0;
  };
  if (starts("ors.order.") || starts("simvenue.order.") ||
      starts("fasttrade.order.") || starts("order.") ||
      event.find("OnRtnOrder") != std::string_view::npos ||
      event.find("OnRspOrder") != std::string_view::npos ||
      event.find("OnErrRtnOrder") != std::string_view::npos ||
      event.find("OnRtnTrade") != std::string_view::npos ||
      event.find("OrderService") != std::string_view::npos) {
    return "order";
  }
  if (starts("ors.md.") || starts("ors.instrument.") || starts("market.") ||
      event.find("MarketData") != std::string_view::npos ||
      event.find("DepthMarket") != std::string_view::npos ||
      event.find("SubMarket") != std::string_view::npos) {
    return "market_data";
  }
  if (starts("ors.rms.") || event.find("Rms") != std::string_view::npos) {
    return "rms";
  }
  if (starts("ors.transport.") ||
      event.find("Aeron") != std::string_view::npos) {
    return "transport";
  }
  if (event.find("Login") != std::string_view::npos ||
      event.find("Auth") != std::string_view::npos ||
      event.find("Front") != std::string_view::npos ||
      event.find("Session") != std::string_view::npos) {
    return "session";
  }
  if (event.find("Query") != std::string_view::npos ||
      event.find("Qry") != std::string_view::npos ||
      event.find("Account") != std::string_view::npos ||
      event.find("Position") != std::string_view::npos) {
    return "account";
  }
  return "system";
}

std::string levelName(spdlog::level::level_enum level) {
  switch (level) {
    case spdlog::level::trace:
      return "TRACE";
    case spdlog::level::debug:
      return "DEBUG";
    case spdlog::level::info:
      return "INFO";
    case spdlog::level::warn:
      return "WARN";
    case spdlog::level::err:
      return "ERROR";
    case spdlog::level::critical:
      return "CRITICAL";
    case spdlog::level::off:
      return "OFF";
    default:
      return "INFO";
  }
}

std::string nowIso8601() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
  std::tm utc{};
  gmtime_r(&seconds, &utc);

  char date[32];
  std::strftime(date, sizeof(date), "%Y-%m-%dT%H:%M:%S", &utc);

  const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
                          now.time_since_epoch()).count() % 1000;
  std::ostringstream out;
  out << date << '.' << std::setw(3) << std::setfill('0') << millis << 'Z';
  return out.str();
}

std::string currentThreadName() {
  char name[32]{};
  if (pthread_getname_np(pthread_self(), name, sizeof(name)) != 0 ||
      name[0] == '\0') {
    return "unknown";
  }
  return name;
}

std::string fieldValue(const LogField& field) {
  if (sensitive(field.key)) {
    return "\"***\"";
  }
  if (std::holds_alternative<std::string>(field.value)) {
    constexpr std::size_t kMaxTextBytes = 4096;
    return "\"" + jsonEscape(
               fitContractField(std::get<std::string>(field.value), kMaxTextBytes)) +
           "\"";
  }
  if (std::holds_alternative<std::int64_t>(field.value)) {
    return std::to_string(std::get<std::int64_t>(field.value));
  }
  if (std::holds_alternative<std::uint64_t>(field.value)) {
    return std::to_string(std::get<std::uint64_t>(field.value));
  }
  if (std::holds_alternative<bool>(field.value)) {
    return std::get<bool>(field.value) ? "true" : "false";
  }
  const double number = std::get<double>(field.value);
  if (!std::isfinite(number)) {
    return "null";
  }
  std::ostringstream out;
  out << std::setprecision(17) << number;
  return out.str();
}

}  // namespace

void logEvent(spdlog::logger& logger, spdlog::level::level_enum level,
              std::string_view event, const LogContext& context,
              std::initializer_list<LogField> fields) {
  std::ostringstream json;
  const auto pid = context.pid != 0 ? context.pid
                                   : static_cast<std::uint32_t>(getpid());
  const std::string host = context.host.empty() ? "unknown" : context.host;
  const std::string service =
      context.service.empty() ? "unknown" : context.service;
  const std::string instance =
      context.instance.empty() ? "unknown" : context.instance;

  bool hasMessage = false;
  bool hasType = false;
  for (const auto& field : fields) {
    hasMessage = hasMessage || field.key == "message";
    hasType = hasType || field.key == "type";
  }

  json << "{\"schema_version\":\"1\",\"ts\":\"" << nowIso8601()
       << "\",\"level\":\"" << levelName(level) << "\",\"service\":\""
       << jsonEscape(service) << "\",\"instance\":\"" << jsonEscape(instance)
       << "\",\"host\":\"" << jsonEscape(host) << "\",\"pid\":" << pid
       << ",\"thread\":\"" << jsonEscape(currentThreadName()) << "\"";
  if (!hasType) {
    json << ",\"type\":\"" << eventType(event) << "\"";
  }
  json << ",\"event\":\"" << jsonEscape(event) << "\"";
  if (!hasMessage) {
    json << ",\"message\":\"" << jsonEscape(event) << "\"";
  }
  for (const auto& field : fields) {
    json << ",\"" << jsonEscape(field.key) << "\":" << fieldValue(field);
  }
  json << '}';
  logger.log(level, "{}", json.str());
}

void logMetrics(spdlog::logger& logger, const Metrics& metrics,
                const LogContext& context) {
  constexpr auto kRelaxed = std::memory_order_relaxed;
  logEvent(logger, spdlog::level::info, "metrics", context,
           {LogField("orders_received", metrics.ordersReceived.load(kRelaxed)),
            LogField("orders_sent", metrics.ordersSent.load(kRelaxed)),
            LogField("ctp_order_callbacks", metrics.ctpOrderCallbacks.load(kRelaxed)),
            LogField("ctp_trade_callbacks", metrics.ctpTradeCallbacks.load(kRelaxed)),
            LogField("aeron_publish_ok", metrics.aeronPublishOk.load(kRelaxed)),
            LogField("aeron_backpressure", metrics.aeronBackPressure.load(kRelaxed)),
            LogField("aeron_failed", metrics.aeronFailed.load(kRelaxed)),
            LogField("unknown_reports", metrics.unknownReports.load(kRelaxed)),
            LogField("stale_front_reports", metrics.staleFrontReports.load(kRelaxed)),
            LogField("md_ticks", metrics.mdTicks.load(kRelaxed)),
            LogField("md_stale", metrics.mdStale.load(kRelaxed)),
            LogField("reconnects", metrics.reconnects.load(kRelaxed)),
            LogField("query_failures", metrics.queryFailures.load(kRelaxed))});
}

std::shared_ptr<spdlog::logger> createLogger(const std::string& name,
                                             const std::string& filePath,
                                             spdlog::level::level_enum level) {
  const std::filesystem::path path(filePath);
  if (path.has_parent_path()) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
      throw std::runtime_error("cannot create log directory " +
                               path.parent_path().string() + ": " + error.message());
    }
  }

  static std::once_flag initOnce;
  std::call_once(initOnce, [] { spdlog::init_thread_pool(8192, 1); });

  auto sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
      filePath, 100ULL * 1024 * 1024, 5);
  auto logger = std::make_shared<spdlog::async_logger>(
      name, sink, spdlog::thread_pool(), spdlog::async_overflow_policy::overrun_oldest);
  logger->set_pattern("%v");
  logger->set_level(level);
  logger->flush_on(spdlog::level::err);
  return logger;
}

}  // namespace ors::base
