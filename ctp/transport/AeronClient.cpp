#include "transport/AeronClient.h"

#include <chrono>
#include <stdexcept>
#include <thread>

#include "concurrent/AtomicBuffer.h"
#include "concurrent/logbuffer/Header.h"

namespace ors {
namespace {

std::shared_ptr<aeron::Publication> awaitPublication(
    aeron::Aeron& aeron, std::int64_t id) {
  for (int attempt = 0; attempt < 200; ++attempt) {
    auto publication = aeron.findPublication(id);
    if (publication) {
      return publication;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  throw std::runtime_error("Aeron publication did not become available");
}

std::shared_ptr<aeron::Subscription> awaitSubscription(
    aeron::Aeron& aeron, std::int64_t id) {
  for (int attempt = 0; attempt < 200; ++attempt) {
    auto subscription = aeron.findSubscription(id);
    if (subscription) {
      return subscription;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  throw std::runtime_error("Aeron subscription did not become available");
}

}  // namespace

AeronClient::AeronClient(const OrsConfig& config) {
  aeron::Context context;
  context.aeronDir(config.aeronDir);
  aeron_ = aeron::Aeron::connect(context);

  for (const int streamId :
       {config.responseStream, config.accountStream, config.marketDataStream,
        config.sessionStream, config.controlStream}) {
    const std::int64_t id = aeron_->addPublication(config.aeronChannel, streamId);
    publications_.emplace(streamId, awaitPublication(*aeron_, id));
  }

  const std::int64_t subscriptionId =
      aeron_->addSubscription(config.aeronChannel, config.requestStream);
  subscription_ = awaitSubscription(*aeron_, subscriptionId);
}

AeronClient::~AeronClient() = default;

bool AeronClient::publish(int streamId, const char* data, std::size_t length) {
  const auto found = publications_.find(streamId);
  if (found == publications_.end()) {
    return false;
  }
  aeron::concurrent::AtomicBuffer buffer(
      reinterpret_cast<std::uint8_t*>(const_cast<char*>(data)), length);
  return found->second->offer(buffer, 0, static_cast<aeron::util::index_t>(length)) > 0;
}

int AeronClient::poll(int fragmentLimit, const Handler& handler) {
  return subscription_->poll(
      [&handler](aeron::concurrent::AtomicBuffer& buffer,
                 aeron::util::index_t offset, aeron::util::index_t length,
                 aeron::concurrent::logbuffer::Header&) {
        handler(1, reinterpret_cast<const char*>(buffer.buffer() + offset),
                static_cast<std::size_t>(length));
      },
      fragmentLimit);
}

}  // namespace ors
