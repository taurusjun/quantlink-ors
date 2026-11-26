#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

#include <Aeron.h>
#include <concurrent/AtomicBuffer.h>
#include <concurrent/logbuffer/Header.h>

#include "com_quantlink_contract_sbe/AccountQuery.h"
#include "com_quantlink_contract_sbe/AccountSnapshot.h"
#include "com_quantlink_contract_sbe/ClientRegister.h"
#include "com_quantlink_contract_sbe/ClientRegisterResponse.h"
#include "com_quantlink_contract_sbe/MarketUpdate.h"
#include "com_quantlink_contract_sbe/MessageHeader.h"
#include "com_quantlink_contract_sbe/OrsRequest.h"
#include "com_quantlink_contract_sbe/OrsResponse.h"
#include "core/OrsConfig.h"

namespace {

using namespace com::quantlink::contract::sbe;
using Clock = std::chrono::steady_clock;

struct Client {
  std::shared_ptr<aeron::Aeron> aeron;
  std::shared_ptr<aeron::Publication> request;
  std::shared_ptr<aeron::Subscription> response;
  std::shared_ptr<aeron::Subscription> control;
  std::shared_ptr<aeron::Subscription> market;
  std::shared_ptr<aeron::Subscription> account;

  std::shared_ptr<aeron::Publication> addPub(const std::string& channel,
                                             int stream) {
    const auto id = aeron->addPublication(channel, stream);
    for (int i = 0; i < 200; ++i) {
      if (auto publication = aeron->findPublication(id)) return publication;
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    throw std::runtime_error("publication timeout");
  }
  std::shared_ptr<aeron::Subscription> addSub(const std::string& channel,
                                              int stream) {
    const auto id = aeron->addSubscription(channel, stream);
    for (int i = 0; i < 200; ++i) {
      if (auto subscription = aeron->findSubscription(id)) return subscription;
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    throw std::runtime_error("subscription timeout");
  }
};

template <typename F>
int poll(aeron::Subscription& subscription, F&& handler, int limit = 10) {
  return subscription.poll(
      [&handler](aeron::concurrent::AtomicBuffer& buffer,
                 aeron::util::index_t offset, aeron::util::index_t length,
                 aeron::concurrent::logbuffer::Header&) {
        handler(reinterpret_cast<const char*>(buffer.buffer() + offset),
                static_cast<std::size_t>(length));
      },
      limit);
}

bool send(aeron::Publication& publication, const char* data, std::size_t len) {
  aeron::concurrent::AtomicBuffer buffer(
      reinterpret_cast<std::uint8_t*>(const_cast<char*>(data)), len);
  return publication.offer(buffer, 0, static_cast<aeron::util::index_t>(len)) >
         0;
}

std::uint16_t templateId(const char* data, std::size_t len) {
  MessageHeader header;
  header.wrap(const_cast<char*>(data), 0, 0, len);
  return header.templateId();
}

std::uint64_t nowNanos() {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count());
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: live_smoke CONFIG\n";
    return 2;
  }

  try {
    const ors::OrsConfig cfg = ors::OrsConfig::load(argv[1]);
    Client client;
    aeron::Context context;
    context.aeronDir(cfg.aeronDir);
    client.aeron = aeron::Aeron::connect(context);
    client.request = client.addPub(cfg.aeronChannel, cfg.requestStream);
    client.response = client.addSub(cfg.aeronChannel, cfg.responseStream);
    client.control = client.addSub(cfg.aeronChannel, cfg.controlStream);
    client.market = client.addSub(cfg.aeronChannel, cfg.marketDataStream);
    client.account = client.addSub(cfg.aeronChannel, cfg.accountStream);
    std::cout << "aeron connected\n";

    char buffer[65536];
    ClientRegister registration;
    registration.wrapAndApplyHeader(buffer, 0, sizeof(buffer))
        .requestId(1)
        .putInstanceId("live-smoke")
        .putAccountId(cfg.accountId)
        .putEnvironment("SHADOW")
        .timestamp(nowNanos());
    if (!send(*client.request, buffer,
              registration.encodedLength() + MessageHeader::encodedLength())) {
      std::cerr << "register send failed\n";
      return 1;
    }

    std::uint32_t clientId = 0;
    const auto registerDeadline = Clock::now() + std::chrono::seconds(10);
    while (Clock::now() < registerDeadline && clientId == 0) {
      poll(*client.control, [&](const char* data, std::size_t len) {
        if (templateId(data, len) != ClientRegisterResponse::sbeTemplateId()) {
          return;
        }
        MessageHeader header;
        header.wrap(const_cast<char*>(data), 0, 0, len);
        ClientRegisterResponse response;
        response.wrapForDecode(const_cast<char*>(data),
                               MessageHeader::encodedLength(),
                               header.blockLength(), header.version(), len);
        std::cout << "register clientId=" << response.clientId()
                  << " error=" << response.errorCode() << '\n';
        clientId = response.clientId();
      });
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if (clientId == 0) {
      std::cerr << "client registration timeout\n";
      return 1;
    }

    double lastPrice = 735.0;
    const auto mdDeadline = Clock::now() + std::chrono::seconds(2);
    while (Clock::now() < mdDeadline) {
      poll(*client.market, [&](const char* data, std::size_t len) {
        if (templateId(data, len) != MarketUpdate::sbeTemplateId()) return;
        MessageHeader header;
        header.wrap(const_cast<char*>(data), 0, 0, len);
        MarketUpdate update;
        update.wrapForDecode(const_cast<char*>(data),
                             MessageHeader::encodedLength(),
                             header.blockLength(), header.version(), len);
        const double price = update.lastPrice() > 0 ? update.lastPrice()
                                                   : update.lastTradedPrice();
        if (price > 0) lastPrice = price;
      });
      if (lastPrice != 735.0) break;
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    std::cout << "lastPrice=" << lastPrice << '\n';

    OrsRequest order;
    order.wrapAndApplyHeader(buffer, 0, sizeof(buffer))
        .contractDescription()
        .putInstrumentName(cfg.instrument)
        .putSymbol(cfg.instrument)
        .expiryDate(0)
        .strikePrice(0)
        .putOptionType(std::string_view("XX"))
        .caLevel(0);
    const std::uint64_t orderId =
        static_cast<std::uint64_t>(clientId) * 1'000'000ULL;
    order.requestType(RequestType::NEW_ORDER)
        .orderType(OrderType::LIMIT)
        .duration(OrderDuration::DAY)
        .priceType(PriceType::PER_UNIT)
        .positionDirection(PositionDirection::OPEN)
        .orderId(static_cast<std::uint32_t>(orderId))
        .token(0)
        .quantity(1)
        .quantityFilled(0)
        .disclosedQty(1)
        .price(lastPrice - 10.0)
        .timestamp(nowNanos())
        .putAccountId(cfg.accountId)
        .transactionType(TransactionType::BUY)
        .exchangeType(57);
    if (!send(*client.request, buffer,
              order.encodedLength() + MessageHeader::encodedLength())) {
      std::cerr << "order send failed\n";
      return 1;
    }

    bool orderResponse = false;
    const auto orderDeadline = Clock::now() + std::chrono::seconds(10);
    while (Clock::now() < orderDeadline && !orderResponse) {
      poll(*client.response, [&](const char* data, std::size_t len) {
        if (templateId(data, len) != OrsResponse::sbeTemplateId()) return;
        MessageHeader header;
        header.wrap(const_cast<char*>(data), 0, 0, len);
        OrsResponse response;
        response.wrapForDecode(const_cast<char*>(data),
                               MessageHeader::encodedLength(),
                               header.blockLength(), header.version(), len);
        std::cout << "order response type="
                  << static_cast<int>(response.responseType())
                  << " orderId=" << response.orderId()
                  << " error=" << response.errorCode() << '\n';
        orderResponse = true;
      });
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if (!orderResponse) {
      std::cerr << "order response timeout\n";
      return 1;
    }

    OrsRequest cancel;
    cancel.wrapAndApplyHeader(buffer, 0, sizeof(buffer))
        .contractDescription()
        .putInstrumentName(cfg.instrument)
        .putSymbol(cfg.instrument)
        .expiryDate(0)
        .strikePrice(0)
        .putOptionType(std::string_view("XX"))
        .caLevel(0);
    cancel.requestType(RequestType::CANCEL_ORDER)
        .orderType(OrderType::LIMIT)
        .duration(OrderDuration::DAY)
        .priceType(PriceType::PER_UNIT)
        .positionDirection(PositionDirection::OPEN)
        .orderId(static_cast<std::uint32_t>(orderId))
        .quantity(1)
        .price(lastPrice - 10.0)
        .timestamp(nowNanos())
        .putAccountId(cfg.accountId)
        .transactionType(TransactionType::BUY)
        .exchangeType(57);
    send(*client.request, buffer,
         cancel.encodedLength() + MessageHeader::encodedLength());

    const auto cancelDeadline = Clock::now() + std::chrono::seconds(10);
    while (Clock::now() < cancelDeadline) {
      bool done = false;
      poll(*client.response, [&](const char* data, std::size_t len) {
        if (templateId(data, len) != OrsResponse::sbeTemplateId()) return;
        MessageHeader header;
        header.wrap(const_cast<char*>(data), 0, 0, len);
        OrsResponse response;
        response.wrapForDecode(const_cast<char*>(data),
                               MessageHeader::encodedLength(),
                               header.blockLength(), header.version(), len);
        std::cout << "cancel response type="
                  << static_cast<int>(response.responseType())
                  << " orderId=" << response.orderId()
                  << " error=" << response.errorCode() << '\n';
        done = true;
      });
      if (done) break;
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    AccountQuery accountQuery;
    accountQuery.wrapAndApplyHeader(buffer, 0, sizeof(buffer))
        .requestId(2)
        .putAccountId(cfg.accountId)
        .timestamp(nowNanos());
    send(*client.request, buffer,
         accountQuery.encodedLength() + MessageHeader::encodedLength());

    const auto accountDeadline = Clock::now() + std::chrono::seconds(10);
    while (Clock::now() < accountDeadline) {
      bool done = false;
      poll(*client.account, [&](const char* data, std::size_t len) {
        if (templateId(data, len) != AccountSnapshot::sbeTemplateId()) return;
        MessageHeader header;
        header.wrap(const_cast<char*>(data), 0, 0, len);
        AccountSnapshot snapshot;
        snapshot.wrapForDecode(const_cast<char*>(data),
                               MessageHeader::encodedLength(),
                               header.blockLength(), header.version(), len);
        std::cout << "account response requestId=" << snapshot.requestId()
                  << " error=" << snapshot.errorCode()
                  << " available=" << snapshot.available() << '\n';
        done = true;
      });
      if (done) return 0;
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    std::cerr << "account response timeout\n";
    return 1;
  } catch (const std::exception& error) {
    std::cerr << "live smoke failed: " << error.what() << '\n';
    return 1;
  }
}
