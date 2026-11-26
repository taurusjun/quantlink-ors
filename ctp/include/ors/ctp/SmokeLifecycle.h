#pragma once

namespace ors::ctp {

/// Test seam for the live smoke lifecycle. One run must log in once, execute
/// both order cases on that session, then query snapshots.
class SmokeLifecycleVenue {
 public:
  virtual ~SmokeLifecycleVenue() = default;
  virtual bool login() = 0;
  virtual bool cancelCase() = 0;
  virtual bool fillCase() = 0;
  virtual bool querySnapshots() = 0;
};

bool runSmokeLifecycle(SmokeLifecycleVenue& venue);

}  // namespace ors::ctp
