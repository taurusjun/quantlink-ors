#pragma once

#include <initializer_list>
#include <memory>
#include <string_view>

#include <spdlog/spdlog.h>

#include "ors/base/Log.h"
#include "ors/base/Metrics.h"

namespace ors::log {

using LogField = base::LogField;

void init(std::shared_ptr<spdlog::logger> logger);
void info(std::string_view event, std::initializer_list<LogField> fields);
void warn(std::string_view event, std::initializer_list<LogField> fields);
void error(std::string_view event, std::initializer_list<LogField> fields);
void metrics(const base::Metrics& metrics);

}  // namespace ors::log
