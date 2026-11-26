#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ors/ctp/ContractStreams.h"

namespace ors::ctp {

struct ContractRuntimeConfig {
  std::string aeronDir;
  std::string venueId;
  std::string orsId;
  std::string environment;
  std::string exchange;
  std::uint64_t sourceSessionId{0};        ///< 0 = generate one per runtime start
  std::uint64_t sourcePublicationEpoch{0};
  std::vector<std::string> instruments;
  ContractStreams streams;
  int orderStream{0};
  int cancelStream{0};
  int queryIntervalSeconds{1};
  std::string logFile{"./log/ors.jsonl"};

  /// Reads the runtime section from the same key=value file as CtpConfig.
  /// Required keys: AERON_DIR, VENUE_ID, ORS_ID, ENVIRONMENT, EXCHANGE,
  /// INSTRUMENTS, STREAM_BASE, SOURCE_PUBLICATION_EPOCH.
  static ContractRuntimeConfig load(const std::string& path);
};

}  // namespace ors::ctp
