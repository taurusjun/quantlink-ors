#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ors/ctp/ContractSink.h"
#include "ors/ctp/CtpConfig.h"

namespace ors::ctp {

struct ContractRuntimeConfig {
  std::string aeronDir;
  std::string venueId{"SIMNOW"};
  std::string orsId{"ors-ctp-1"};
  std::string environment{"SHADOW"};
  std::string exchange{"SHFE"};
  std::vector<std::string> instruments;
  ContractStreams streams{9004, 9005, 9006, 9007, 9001};
  int orderStream{9002};
  int cancelStream{9003};
  int queryIntervalSeconds{1};
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
