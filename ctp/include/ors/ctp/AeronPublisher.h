#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Aeron.h"
#include "ors/ctp/ContractSink.h"

namespace ors::ctp {

/// Thin Aeron C++ implementation of the ContractSink transport seam.
class AeronFramePublisher final : public FramePublisher {
 public:
  AeronFramePublisher(std::string aeronDir, std::string channel,
                      const std::vector<int>& streamIds);
  ~AeronFramePublisher() override;

  AeronFramePublisher(const AeronFramePublisher&) = delete;
  AeronFramePublisher& operator=(const AeronFramePublisher&) = delete;

  bool publish(int streamId, const char* data, std::size_t length) override;

 private:
  std::shared_ptr<aeron::Aeron> aeron_;
  std::map<int, std::shared_ptr<aeron::Publication>> publications_;
};

}  // namespace ors::ctp
