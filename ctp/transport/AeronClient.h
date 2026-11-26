#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Aeron.h"
#include "core/OrsConfig.h"
#include "transport/Frame.h"

namespace ors {

class AeronClient final : public FramePublisher, public FrameSubscriber {
 public:
  explicit AeronClient(const OrsConfig& config);
  ~AeronClient() override;

  bool publish(int streamId, const char* data, std::size_t length) override;
  int poll(int fragmentLimit, const Handler& handler) override;

 private:
  std::shared_ptr<aeron::Aeron> aeron_;
  std::map<int, std::shared_ptr<aeron::Publication>> publications_;
  std::shared_ptr<aeron::Subscription> subscription_;
};

}  // namespace ors
