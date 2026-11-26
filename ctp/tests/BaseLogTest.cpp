#include <iostream>
#include <sstream>
#include <string>

#include <spdlog/sinks/ostream_sink.h>
#include <spdlog/spdlog.h>

#include "ors/base/Log.h"
#include "ors/base/Metrics.h"

int main() {
  std::ostringstream output;
  auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(output);
  spdlog::logger logger("test", sink);
  logger.set_pattern("%v");
  logger.set_level(spdlog::level::trace);

  ors::base::logEvent(logger, spdlog::level::info, "order.rtn",
      {ors::base::LogField("order_ref", 42),
       ors::base::LogField("reason", "a\"b\n"),
       ors::base::LogField("password", "secret"),
       ors::base::LogField("price", 735.5),
       ors::base::LogField("ok", true)});

  ors::base::Metrics metrics;
  metrics.ordersReceived.fetch_add(3);
  metrics.mdTicks.fetch_add(7);
  ors::base::logMetrics(logger, metrics);

  const std::string text = output.str();
  if (text.find("\"order_ref\":42") == std::string::npos) {
    std::cerr << "FAIL: numeric order_ref\n";
    return 1;
  }
  if (text.find("\"reason\":\"a\\\"b\\n\"") == std::string::npos) {
    std::cerr << "FAIL: JSON string escaping\n";
    return 1;
  }
  if (text.find("secret") != std::string::npos) {
    std::cerr << "FAIL: credential leaked\n";
    return 1;
  }
  if (text.find("\"price\":735.5") == std::string::npos ||
      text.find("\"ok\":true") == std::string::npos) {
    std::cerr << "FAIL: typed JSON values\n";
    return 1;
  }
  if (text.find("\"event\":\"metrics\"") == std::string::npos ||
      text.find("\"orders_received\":3") == std::string::npos) {
    std::cerr << "FAIL: metrics JSONL\n";
    return 1;
  }
  if (text.empty() || text.front() != '{') {
    std::cerr << "FAIL: JSONL object\n";
    return 1;
  }
  std::cout << "base_log: JSONL, escaping and redaction\n";
  return 0;
}
