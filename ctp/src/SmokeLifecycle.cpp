#include "ors/ctp/SmokeLifecycle.h"

namespace ors::ctp {

bool runSmokeLifecycle(SmokeLifecycleVenue& venue) {
  if (!venue.login()) return false;
  if (!venue.cancelCase()) return false;
  if (!venue.fillCase()) return false;
  return venue.querySnapshots();
}

}  // namespace ors::ctp
