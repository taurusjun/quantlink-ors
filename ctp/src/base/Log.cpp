#include "ors/base/Log.h"

#include <cctype>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>

#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>

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
  std::tm local{};
  localtime_r(&seconds, &local);

  char date[32];
  std::strftime(date, sizeof(date), "%Y-%m-%dT%H:%M:%S", &local);
  char zone[8];
  std::strftime(zone, sizeof(zone), "%z", &local);

  const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
                          now.time_since_epoch()).count() % 1000;
  std::ostringstream out;
  out << date << '.' << std::setw(3) << std::setfill('0') << millis << zone;
  return out.str();
}

std::string fieldValue(const LogField& field) {
  if (sensitive(field.key)) {
    return "\"***\"";
  }
  if (std::holds_alternative<std::string>(field.value)) {
    return "\"" + jsonEscape(std::get<std::string>(field.value)) + "\"";
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
              std::string_view event, std::initializer_list<LogField> fields) {
  std::ostringstream json;
  json << "{\"ts\":\"" << nowIso8601() << "\",\"level\":\""
       << levelName(level) << "\",\"event\":\"" << jsonEscape(event) << "\"";
  for (const auto& field : fields) {
    json << ",\"" << jsonEscape(field.key) << "\":" << fieldValue(field);
  }
  json << '}';
  logger.log(level, "{}", json.str());
}

void logMetrics(spdlog::logger& logger, const Metrics& metrics) {
  constexpr auto kRelaxed = std::memory_order_relaxed;
  logEvent(logger, spdlog::level::info, "metrics",
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
