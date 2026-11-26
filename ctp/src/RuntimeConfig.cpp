#include "ors/ctp/RuntimeConfig.h"

#include <limits>
#include <stdexcept>
#include <string>

#include "ors/base/ConfigFile.h"
#include "ors/base/StringList.h"

namespace ors::ctp {
namespace {

constexpr int kOrderStreamOffset = 2;
constexpr int kCancelStreamOffset = 3;
constexpr int kMaxStreamOffset = 7;

std::uint64_t requireUnsigned(const base::ConfigFile& file, const std::string& key) {
  const std::string text = file.require(key);
  if (!text.empty() && text.front() == '-') {
    throw std::runtime_error("ors runtime config: " + key + " must be an unsigned integer");
  }
  std::size_t used = 0;
  unsigned long long value = 0;
  try {
    value = std::stoull(text, &used);
  } catch (const std::exception&) {
    throw std::runtime_error("ors runtime config: " + key + " must be an unsigned integer");
  }
  if (used != text.size()) {
    throw std::runtime_error("ors runtime config: " + key + " must be an unsigned integer");
  }
  return static_cast<std::uint64_t>(value);
}

int requirePositiveInt(const base::ConfigFile& file, const std::string& key) {
  const std::string text = file.require(key);
  std::size_t used = 0;
  int value = 0;
  try {
    value = std::stoi(text, &used);
  } catch (const std::exception&) {
    throw std::runtime_error("ors runtime config: " + key + " must be a positive integer");
  }
  if (used != text.size() || value <= 0) {
    throw std::runtime_error("ors runtime config: " + key + " must be a positive integer");
  }
  return value;
}

}  // namespace

ContractRuntimeConfig ContractRuntimeConfig::load(const std::string& path) {
  const base::ConfigFile file = base::ConfigFile::load(path, "ors runtime config");

  ContractRuntimeConfig config;
  config.aeronDir = file.require("AERON_DIR");
  config.venueId = file.require("VENUE_ID");
  config.orsId = file.require("ORS_ID");
  config.environment = file.require("ENVIRONMENT");
  config.exchange = file.require("EXCHANGE");
  config.instruments = base::splitList(file.require("INSTRUMENTS"));
  if (config.instruments.empty()) {
    throw std::runtime_error("ors runtime config: INSTRUMENTS is required");
  }
  config.sourcePublicationEpoch = requireUnsigned(file, "SOURCE_PUBLICATION_EPOCH");
  if (file.contains("SOURCE_SESSION_ID")) {
    config.sourceSessionId = requireUnsigned(file, "SOURCE_SESSION_ID");
  }

  const int streamBase = requirePositiveInt(file, "STREAM_BASE");
  if (streamBase > std::numeric_limits<int>::max() - kMaxStreamOffset) {
    throw std::runtime_error("ors runtime config: STREAM_BASE is too large");
  }
  config.orderStream = streamBase + kOrderStreamOffset;
  config.cancelStream = streamBase + kCancelStreamOffset;
  config.streams = ContractStreams::fromBase(streamBase);

  if (file.contains("QUERY_INTERVAL_SECONDS")) {
    config.queryIntervalSeconds = requirePositiveInt(file, "QUERY_INTERVAL_SECONDS");
  }
  if (file.contains("LOG_FILE")) {
    config.logFile = file.require("LOG_FILE");
  }
  return config;
}

}  // namespace ors::ctp
