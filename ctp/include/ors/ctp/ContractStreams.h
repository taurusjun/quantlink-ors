#pragma once

namespace ors::ctp {

/// Aeron stream ids for one contract session. Deployment overrides the base
/// through STREAM_BASE; the relative layout is fixed here.
struct ContractStreams {
  int execution{2003};
  int position{2004};
  int account{2005};
  int session{2006};
  int marketData{1001};

  /// base+1 market data, base+4..+7 execution/position/account/session.
  static ContractStreams fromBase(int base) {
    return {base + 4, base + 5, base + 6, base + 7, base + 1};
  }
};

}  // namespace ors::ctp
