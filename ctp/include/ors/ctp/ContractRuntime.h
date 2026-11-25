#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "ors/ctp/ContractSink.h"
#include "ors/ctp/CtpConfig.h"

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

  /// Reads the runtime section from the same key=value file as CtpConfig.
  /// Required keys: AERON_DIR, VENUE_ID, ORS_ID, ENVIRONMENT, EXCHANGE,
  /// INSTRUMENTS, STREAM_BASE, SOURCE_PUBLICATION_EPOCH.
  static ContractRuntimeConfig load(const std::string& path);
};

/// Long-running ORS loop: CTP callbacks -> ContractSink/Aeron, and L1 commands
/// -> CtpAdapter.
class ContractRuntime {
 public:
  ContractRuntime(CtpConfig config, ContractRuntimeConfig runtimeConfig);
  ~ContractRuntime();

  ContractRuntime(const ContractRuntime&) = delete;
  ContractRuntime& operator=(const ContractRuntime&) = delete;

  int run();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace ors::ctp
