#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "ors/ctp/ContractSink.h"
#include "ors/ctp/CtpConfig.h"
#include "ors/ctp/RuntimeConfig.h"

namespace ors::ctp {

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
