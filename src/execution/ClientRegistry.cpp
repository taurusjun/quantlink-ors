#include "execution/ClientRegistry.h"

#include <fcntl.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace ors {

bool ClientRegistry::configureHighWater(std::string path) {
  highWaterPath_ = std::move(path);
  if (highWaterPath_.empty()) {
    return true;
  }

  std::ifstream input(highWaterPath_);
  if (!input) {
    return true;
  }

  std::uint64_t value = 0;
  if (!(input >> value) || value == 0) {
    return false;
  }
  nextClientId_ = value > kMaxClientId + 1
                      ? kMaxClientId + 1
                      : static_cast<std::uint32_t>(value);
  return true;
}

bool ClientRegistry::persistHighWater() const {
  if (highWaterPath_.empty()) {
    return true;
  }

  const std::filesystem::path path(highWaterPath_);
  std::error_code error;
  const auto parent = path.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, error);
    if (error) {
      return false;
    }
  }

  const std::string temporary = highWaterPath_ + ".tmp";
  const std::string contents = std::to_string(nextClientId_) + "\n";
  const int fd = ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) {
    return false;
  }

  std::size_t written = 0;
  while (written < contents.size()) {
    const ssize_t result =
        ::write(fd, contents.data() + written, contents.size() - written);
    if (result <= 0) {
      ::close(fd);
      ::unlink(temporary.c_str());
      return false;
    }
    written += static_cast<std::size_t>(result);
  }
  if (::fsync(fd) != 0) {
    ::close(fd);
    ::unlink(temporary.c_str());
    return false;
  }
  ::close(fd);

  std::filesystem::rename(temporary, path, error);
  if (error) {
    ::unlink(temporary.c_str());
    return false;
  }
  return true;
}

ClientRegistration ClientRegistry::registerClient(std::string_view) {
  if (nextClientId_ > kMaxClientId) {
    return {false, 0, 1, "client id range exhausted"};
  }
  const std::uint32_t clientId = nextClientId_++;
  if (!persistHighWater()) {
    return {false, 0, 2, "client id highwater write failed"};
  }
  allocated_.insert(clientId);
  return {true, clientId, 0, {}};
}

void ClientRegistry::releaseIfFlat(
    std::uint32_t clientId, const std::set<std::uint32_t>& orderIds) {
  for (const std::uint32_t orderId : orderIds) {
    if (clientIdFromOrderId(orderId) == clientId) {
      return;
    }
  }
  allocated_.erase(clientId);
}

std::uint32_t ClientRegistry::makeOrderId(std::uint32_t clientId,
                                          std::uint32_t orderCount) {
  if (clientId == 0 || clientId > kMaxClientId ||
      orderCount >= kOrderIdRange) {
    return 0;
  }
  return clientId * kOrderIdRange + orderCount;
}

std::uint32_t ClientRegistry::clientIdFromOrderId(std::uint32_t orderId) {
  return orderId / kOrderIdRange;
}

bool ClientRegistry::hasClientId(std::uint32_t clientId) const {
  return allocated_.count(clientId) != 0;
}

bool ClientRegistry::ownsOrder(std::uint32_t orderId) const {
  return orderId != 0 && hasClientId(clientIdFromOrderId(orderId));
}

}  // namespace ors
