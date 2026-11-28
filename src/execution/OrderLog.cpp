#include "execution/OrderLog.h"

#include <string>

#include "infra/LogFacade.h"

namespace ors {
namespace {

std::string trim(const char* value) {
  if (value == nullptr) {
    return "";
  }
  std::string text(value);
  const auto first = text.find_first_not_of(" \t\r\n");
  const auto last = text.find_last_not_of(" \t\r\n");
  return first == std::string::npos ? "" : text.substr(first, last - first + 1);
}

std::string exchangeName(unsigned char exchange) {
  switch (exchange) {
    case 57:
      return "SHFE";
    case 58:
      return "CFFEX";
    case 59:
      return "ZCE";
    case 60:
      return "DCE";
    case 61:
      return "GFEX";
    case 70:
      return "SH";
    case 71:
      return "SZ";
    case 72:
      return "INE";
    default:
      return "UNKNOWN";
  }
}

std::string sideName(unsigned char side) {
  switch (side) {
    case 'B':
      return "BUY";
    case 'S':
      return "SELL";
    default:
      return "UNKNOWN";
  }
}

std::string offsetName(int qtyType) {
  switch (qtyType) {
    case 1:
      return "CLOSE_TODAY";
    case 2:
      return "CLOSE_YESTERDAY";
    case 3:
      return "OPEN";
    default:
      return "UNKNOWN";
  }
}

}  // namespace

void logOrderEvent(const char* event, const char* message,
                   const OrderInfo& order, const char* status,
                   OrderLogLevel level, std::int64_t errorCode, double price,
                   std::uint64_t quantity, std::uint64_t filledQuantity,
                   std::uint64_t remainingQuantity,
                   const char* errorMessage, const char* source,
                   const char* rawStatus, const char* rawStatusName,
                   const char* exchangeTradeId) {
  const std::string errorCodeText =
      errorCode == 0 ? "" : std::to_string(errorCode);
  const auto fields = std::initializer_list<log::LogField>{
      {"type", "order"},
      {"order_id", std::to_string(order.localID)},
      {"strategy_id", order.strategyId},
      {"order_ref", trim(order.exchID)},
      {"exchange_order_id", trim(order.OrderSysID)},
      {"front_id", order.frontId},
      {"session_id", order.sessionId},
      {"exchange_id", exchangeName(static_cast<unsigned char>(order.exchange))},
      {"instrument", trim(order.ScripCode)},
      {"account_id", trim(order.ClientID)},
      {"side", sideName(static_cast<unsigned char>(order.Transaction_Type))},
      {"offset", offsetName(order.qtyType)},
      {"status", status == nullptr ? "" : status},
      {"source", source == nullptr ? "" : source},
      {"raw_status", rawStatus == nullptr ? "" : rawStatus},
      {"raw_status_name", rawStatusName == nullptr ? "" : rawStatusName},
      {"exchange_trade_id", exchangeTradeId == nullptr ? "" : exchangeTradeId},
      {"error_code", errorCodeText},
      {"error_message", errorMessage == nullptr ? "" : errorMessage},
      {"price", price},
      {"quantity", quantity},
      {"filled_quantity", filledQuantity},
      {"remaining_quantity", remainingQuantity},
      {"message", message == nullptr ? "" : message}};

  switch (level) {
    case OrderLogLevel::Error:
      log::error(event, fields);
      break;
    case OrderLogLevel::Warn:
      log::warn(event, fields);
      break;
    default:
      log::info(event, fields);
      break;
  }
}

}  // namespace ors
