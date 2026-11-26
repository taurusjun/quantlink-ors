#pragma once

#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <variant>

#include <spdlog/spdlog.h>

namespace ors::base {

struct Metrics;

/// One JSONL field. Numeric values stay JSON numbers.
struct LogField {
  std::string key;
  std::variant<std::string, std::int64_t, std::uint64_t, double, bool> value;

  LogField(std::string fieldKey, std::string fieldValue)
      : key(std::move(fieldKey)), value(std::move(fieldValue)) {}
  LogField(std::string fieldKey, const char* fieldValue)
      : key(std::move(fieldKey)), value(std::string(fieldValue)) {}
  LogField(std::string fieldKey, int fieldValue)
      : key(std::move(fieldKey)), value(static_cast<std::int64_t>(fieldValue)) {}
  LogField(std::string fieldKey, std::int64_t fieldValue)
      : key(std::move(fieldKey)), value(fieldValue) {}
  LogField(std::string fieldKey, std::uint64_t fieldValue)
      : key(std::move(fieldKey)), value(fieldValue) {}
  LogField(std::string fieldKey, double fieldValue)
      : key(std::move(fieldKey)), value(fieldValue) {}
  LogField(std::string fieldKey, bool fieldValue)
      : key(std::move(fieldKey)), value(fieldValue) {}
};

void logEvent(spdlog::logger& logger, spdlog::level::level_enum level,
              std::string_view event, std::initializer_list<LogField> fields);

void logMetrics(spdlog::logger& logger, const Metrics& metrics);

}  // namespace ors::base
