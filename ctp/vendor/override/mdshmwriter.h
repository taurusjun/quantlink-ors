#pragma once

#include <cstddef>

#include "marketupdateNew.h"

class MarketDataShmWriter {
 public:
  void init(int, std::size_t) {}
  void write(illuminati::md::MarketUpdateNew*) {}
};
