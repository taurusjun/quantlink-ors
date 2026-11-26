#pragma once

#include <mutex>

namespace illuminati {
namespace ds {

class CasLock {
 public:
  void lock() { mutex_.lock(); }
  void unlock() { mutex_.unlock(); }

 private:
  std::mutex mutex_;
};

}  // namespace ds
}  // namespace illuminati
