#include "infra/LogFacade.h"

#include <iostream>
#include <mutex>

namespace ors::log {
namespace {

std::shared_ptr<spdlog::logger> g_logger;
base::LogContext g_context;
std::mutex g_initMutex;

void emit(spdlog::level::level_enum level, std::string_view event,
          std::initializer_list<LogField> fields) {
  if (!g_logger) {
    return;
  }
  base::logEvent(*g_logger, level, event, g_context, fields);
}

}  // namespace

void init(std::shared_ptr<spdlog::logger> logger,
          const base::LogContext& context) {
  std::lock_guard<std::mutex> lock(g_initMutex);
  g_context = context;
  g_logger = std::move(logger);
}

void info(std::string_view event, std::initializer_list<LogField> fields) {
  emit(spdlog::level::info, event, fields);
}

void warn(std::string_view event, std::initializer_list<LogField> fields) {
  emit(spdlog::level::warn, event, fields);
}

void error(std::string_view event, std::initializer_list<LogField> fields) {
  emit(spdlog::level::err, event, fields);
}

void metrics(const base::Metrics& metrics) {
  if (!g_logger) {
    return;
  }
  base::logMetrics(*g_logger, metrics, g_context);
}

LegacyLine::LegacyLine(Level level, std::string event)
    : level_(level), event_(std::move(event)) {}

LegacyLine::~LegacyLine() {
  if (!g_logger) {
    return;
  }
  const auto converted = level_ == Level::FATAL
                             ? spdlog::level::critical
                             : level_ == Level::ERROR
                                   ? spdlog::level::err
                                   : level_ == Level::WARN
                                         ? spdlog::level::warn
                                         : spdlog::level::info;
  base::logEvent(*g_logger, converted, event_, g_context,
                 {LogField("message", stream_.str())});
}

LegacyLine line(Level level, std::string event) {
  return LegacyLine(level, std::move(event));
}

}  // namespace ors::log
