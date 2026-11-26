#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace illuminati {

class ITime_ClockRT {
 public:
  static std::uint64_t GetCurrentTime() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
  }
};

class ITime_Logger {
 public:
  static std::string GetCurrentYYYYMMDD() {
    return "19700101";
  }
};

}  // namespace illuminati
