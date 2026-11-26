#include <cstring>
#include <iostream>
#include <vector>

#include "core/MDHandler.h"

namespace {

int failures = 0;

void check(bool condition, const char* what) {
  if (!condition) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

struct Sink final : ors::MarketDataSink {
  std::vector<illuminati::md::MarketUpdateNew> updates;
  bool accept{true};
  bool send(const illuminati::md::MarketUpdateNew& update) override {
    updates.push_back(update);
    return accept;
  }
};

illuminati::md::MarketUpdateNew sample() {
  illuminati::md::MarketUpdateNew update{};
  std::strcpy(update.m_symbol, "au2612");
  update.m_exchTS = 100;
  update.m_validBids = 20;
  update.m_validAsks = 20;
  for (int i = 0; i < 20; ++i) {
    update.m_bidUpdates[i].price = 735.0 - i;
    update.m_bidUpdates[i].quantity = i + 1;
    update.m_bidUpdates[i].orderCount = i + 2;
    update.m_askUpdates[i].price = 736.0 + i;
    update.m_askUpdates[i].quantity = i + 3;
    update.m_askUpdates[i].orderCount = i + 4;
  }
  return update;
}

void preservesTwentyLevels() {
  Sink sink;
  MDHandler handler(sink);
  auto update = sample();
  handler.ProcessMDUpdate(reinterpret_cast<char*>(&update));
  check(sink.updates.size() == 1, "one tick reaches sink");
  const auto& delivered = sink.updates.front();
  check(delivered.m_bidUpdates[0].price == 735.0 &&
            delivered.m_bidUpdates[19].price == 716.0,
        "bid levels preserved");
  check(delivered.m_askUpdates[0].price == 736.0 &&
            delivered.m_askUpdates[19].price == 755.0,
        "ask levels preserved");
}

void incrementsSequenceAndTimestamp() {
  Sink sink;
  MDHandler handler(sink);
  auto first = sample();
  auto second = sample();
  handler.ProcessMDUpdate(reinterpret_cast<char*>(&first));
  handler.ProcessMDUpdate(reinterpret_cast<char*>(&second));
  check(sink.updates[0].m_seqnum == 1 && sink.updates[1].m_seqnum == 2,
        "sequence increments");
  check(sink.updates[0].m_timestamp != 0, "timestamp set");
}

}  // namespace

int main() {
  preservesTwentyLevels();
  incrementsSequenceAndTimestamp();
  return failures == 0 ? 0 : 1;
}
