#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

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

  const auto path = std::filesystem::temp_directory_path() / "ors_base_log_test.jsonl";
  std::error_code ignored;
  std::filesystem::remove(path, ignored);
  auto fileLogger = ors::base::createLogger("file-test", path.string());
  ors::base::logEvent(*fileLogger, spdlog::level::info, "file.test",
                      {ors::base::LogField("n", 7)});
  fileLogger->flush();
  std::string fileText;
  for (int attempt = 0; attempt < 100; ++attempt) {
    std::ifstream input(path, std::ios::binary);
    fileText.assign(std::istreambuf_iterator<char>(input),
                    std::istreambuf_iterator<char>());
    if (fileText.find("\"event\":\"file.test\"") != std::string::npos) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  fileLogger.reset();
  std::filesystem::remove(path, ignored);
  if (fileText.find("\"event\":\"file.test\"") == std::string::npos ||
      fileText.find("\"n\":7") == std::string::npos) {
    std::cerr << "FAIL: async file logger\n";
    return 1;
  }
  std::cout << "base_log: JSONL, escaping, redaction and async file\n";
  return 0;
}
