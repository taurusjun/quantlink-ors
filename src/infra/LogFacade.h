#pragma once

#include <initializer_list>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>

#include <spdlog/spdlog.h>

#include "ors/base/Log.h"
#include "ors/base/Metrics.h"

namespace ors::log {

using LogField = base::LogField;

void init(std::shared_ptr<spdlog::logger> logger,
          const base::LogContext& context);
void info(std::string_view event, std::initializer_list<LogField> fields);
void warn(std::string_view event, std::initializer_list<LogField> fields);
void error(std::string_view event, std::initializer_list<LogField> fields);
void metrics(const base::Metrics& metrics);

enum class Level { INFO, WARN, ERROR, FATAL };

class LegacyLine {
 public:
  LegacyLine(Level level, std::string event);
  ~LegacyLine();
  LegacyLine(const LegacyLine&) = delete;
  LegacyLine& operator=(const LegacyLine&) = delete;

  template <typename T>
  LegacyLine& operator<<(const T& value) {
    stream_ << value;
    return *this;
  }
  LegacyLine& operator<<(std::ostream& (*manipulator)(std::ostream&)) {
    stream_ << manipulator;
    return *this;
  }

 private:
  Level level_;
  std::string event_;
  std::ostringstream stream_;
};

LegacyLine line(Level level, std::string event);

}  // namespace ors::log

#define ORS_LOG(level) \
  ::ors::log::line(::ors::log::Level::level, __FUNCTION__)
