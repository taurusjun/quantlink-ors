#pragma once

#include "rmsimpl.hpp"

namespace ors {

/// Owns the vendor RMS implementation. The session talks to RMS only through
/// this boundary; RMS policy remains untouched.
class RmsGate {
 public:
  RmsImpl& impl() { return rms_; }
  const RmsImpl& impl() const { return rms_; }

 private:
  RmsImpl rms_;
};

}  // namespace ors
