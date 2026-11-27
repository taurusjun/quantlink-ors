#pragma once

#include <cstddef>
#include <functional>

namespace ors {

class FramePublisher {
 public:
  virtual ~FramePublisher() = default;
  virtual bool publish(int streamId, const char* data, std::size_t length) = 0;
};

class FrameSubscriber {
 public:
  using Handler =
      std::function<void(int streamId, const char* data, std::size_t length)>;
  virtual ~FrameSubscriber() = default;
  virtual int poll(int fragmentLimit, const Handler& handler) = 0;
};

}  // namespace ors
