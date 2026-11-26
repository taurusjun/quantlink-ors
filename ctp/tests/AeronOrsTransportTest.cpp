#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "com_quantlink_contract_sbe/AccountQuery.h"
#include "com_quantlink_contract_sbe/ClientRegister.h"
#include "com_quantlink_contract_sbe/MessageHeader.h"
#include "com_quantlink_contract_sbe/OrsRequest.h"
#include "transport/AeronOrsTransport.h"

namespace {

int failures = 0;

void check(bool condition, const char* what) {
  if (!condition) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

struct FakePublisher final : ors::FramePublisher {
  std::vector<std::pair<int, std::vector<char>>> frames;
  bool accept{true};
  bool publish(int streamId, const char* data, std::size_t length) override {
    frames.emplace_back(streamId,
                        std::vector<char>(data, data + length));
    return accept;
  }
};

struct FakeSubscriber final : ors::FrameSubscriber {
  std::vector<std::pair<int, std::vector<char>>> frames;
  int poll(int, const Handler& handler) override {
    for (const auto& frame : frames) {
      handler(frame.first, frame.second.data(), frame.second.size());
    }
    return static_cast<int>(frames.size());
  }
};

ors::OrsConfig config() {
  ors::OrsConfig config;
  config.requestStream = 1;
  config.responseStream = 2;
  config.accountStream = 3;
  config.marketDataStream = 4;
  config.sessionStream = 5;
  config.controlStream = 6;
  return config;
}

void responseUsesResponseStream() {
  FakePublisher publisher;
  FakeSubscriber subscriber;
  ors::AeronOrsTransport transport(config(), publisher, subscriber);
  illuminati::infra::ResponseMsg response;
  response.Response_Type = illuminati::infra::NEW_ORDER_CONFIRM;
  response.OrderID = 7;
  check(transport.sendResponse(response), "response publish");
  check(publisher.frames.size() == 1 && publisher.frames[0].first == 2,
        "response uses stream 2");
}

void accountUsesAccountStream() {
  FakePublisher publisher;
  FakeSubscriber subscriber;
  ors::AeronOrsTransport transport(config(), publisher, subscriber);
  ors::AccountSnapshot snapshot;
  snapshot.requestId = 9;
  check(transport.sendAccount(snapshot), "account publish");
  check(publisher.frames.size() == 1 && publisher.frames[0].first == 3,
        "account uses stream 3");
}

void requestRoutesToHandler() {
  FakePublisher publisher;
  FakeSubscriber subscriber;
  char buffer[2048];
  com::quantlink::contract::sbe::OrsRequest message;
  message.wrapAndApplyHeader(buffer, 0, sizeof(buffer))
      .contractDescription()
      .putInstrumentName("au2612")
      .putSymbol("au2612")
      .expiryDate(0)
      .strikePrice(0)
      .putOptionType(std::string_view("XX"))
      .caLevel(0);
  message.orderId(77)
      .quantity(1)
      .price(735.5)
      .timestamp(1)
      .requestType(com::quantlink::contract::sbe::RequestType::NEW_ORDER)
      .orderType(com::quantlink::contract::sbe::OrderType::LIMIT)
      .duration(com::quantlink::contract::sbe::OrderDuration::DAY)
      .priceType(com::quantlink::contract::sbe::PriceType::PER_UNIT)
      .positionDirection(com::quantlink::contract::sbe::PositionDirection::OPEN)
      .transactionType(com::quantlink::contract::sbe::TransactionType::BUY)
      .exchangeType(57)
      .putAccountId("142266");
  subscriber.frames.emplace_back(
      1, std::vector<char>(buffer, buffer + message.encodedLength() +
                                        com::quantlink::contract::sbe::MessageHeader::encodedLength()));

  ors::AeronOrsTransport transport(config(), publisher, subscriber);
  illuminati::infra::RequestMsg received{};
  bool called = false;
  transport.start(
      [&](illuminati::infra::RequestMsg* request) {
        called = true;
        received = *request;
      },
      [](const ors::AccountQueryRequest&) {},
      [](const ors::ClientRegisterRequest&) {});
  transport.poll(10);
  check(called && received.OrderID == 77, "request routed");
}

void accountQueryRoutesToHandler() {
  FakePublisher publisher;
  FakeSubscriber subscriber;
  char buffer[1024];
  com::quantlink::contract::sbe::AccountQuery message;
  message.wrapAndApplyHeader(buffer, 0, sizeof(buffer))
      .requestId(9)
      .putAccountId("142266")
      .timestamp(1);
  subscriber.frames.emplace_back(
      1, std::vector<char>(buffer, buffer + message.encodedLength() +
                                        com::quantlink::contract::sbe::MessageHeader::encodedLength()));

  ors::AeronOrsTransport transport(config(), publisher, subscriber);
  std::uint64_t received = 0;
  transport.start([](illuminati::infra::RequestMsg*) {},
                  [&](const ors::AccountQueryRequest& query) {
                    received = query.requestId;
                  },
                  [](const ors::ClientRegisterRequest&) {});
  transport.poll(10);
  check(received == 9, "account query routed");
}

void backPressureIsReported() {
  FakePublisher publisher;
  publisher.accept = false;
  FakeSubscriber subscriber;
  ors::AeronOrsTransport transport(config(), publisher, subscriber);
  illuminati::infra::ResponseMsg response;
  check(!transport.sendResponse(response), "back pressure reported");
}

}  // namespace

int main() {
  responseUsesResponseStream();
  accountUsesAccountStream();
  requestRoutesToHandler();
  accountQueryRoutesToHandler();
  backPressureIsReported();
  return failures == 0 ? 0 : 1;
}
