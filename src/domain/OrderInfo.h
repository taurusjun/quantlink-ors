#pragma once

#include <cstdint>
#include <map>
#include <string>

#include "orderresponse.h"

namespace ors {

struct OrderInfo {
  ::ors::infra::RequestType requestType;
  bool orderSent;
  unsigned long localID;
  char exchID[31];
  char OrderSysID[21];
  char ClientID[11];
  char ScripCode[31];
  unsigned char Transaction_Type;
  double Price;
  unsigned long Qty;
  double oldPrice;
  unsigned long oldQty;
  unsigned long ModQtySent;
  unsigned long openQty;
  std::uint64_t sendTime;
  int frontId;
  int sessionId;
  char exchange;
  int qtyType;
};

using OrderAccountMap = std::map<unsigned long, OrderInfo>;
using OrderAccountMapIter = OrderAccountMap::iterator;
using OrderAccountMap2 = std::map<std::string, std::uint32_t>;
using OrderAccountMapIter2 = OrderAccountMap2::iterator;

}  // namespace ors
