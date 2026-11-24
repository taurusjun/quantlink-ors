#include "ors/ctp/AeronPublisher.h"

#include <chrono>
#include <stdexcept>
#include <thread>

#include "concurrent/AtomicBuffer.h"

namespace ors::ctp {

AeronFramePublisher::AeronFramePublisher(
    std::string aeronDir, std::string channel, const std::vector<int>& streamIds) {
  aeron::Context context;
  context.aeronDir(aeronDir);
  aeron_ = aeron::Aeron::connect(context);
  for (const int streamId : streamIds) {
    const std::int64_t id = aeron_->addPublication(channel, streamId);
    std::shared_ptr<aeron::Publication> publication;
    for (int attempt = 0; attempt < 200 && !publication; ++attempt) {
      publication = aeron_->findPublication(id);
      if (!publication) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
    }
    if (!publication) {
      throw std::runtime_error("Aeron publication did not become available for stream "
                               + std::to_string(streamId));
    }
    publications_.emplace(streamId, std::move(publication));
  }
}

AeronFramePublisher::~AeronFramePublisher() = default;

bool AeronFramePublisher::publish(int streamId, const char* data, std::size_t length) {
  const auto publication = publications_.find(streamId);
  if (publication == publications_.end()) return false;
  aeron::AtomicBuffer buffer(
      reinterpret_cast<std::uint8_t*>(const_cast<char*>(data)), length);
  return publication->second->offer(buffer, 0, static_cast<aeron::index_t>(length)) > 0;
}

}  // namespace ors::ctp
