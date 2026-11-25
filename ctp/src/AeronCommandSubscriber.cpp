#include "ors/ctp/AeronCommandSubscriber.h"

#include <chrono>
#include <stdexcept>
#include <thread>

#include "com_quantlink_contract_sbe/CancelRequest.h"
#include "com_quantlink_contract_sbe/MessageHeader.h"
#include "com_quantlink_contract_sbe/OrderRequest.h"

namespace ors::ctp {
namespace sbe = com::quantlink::contract::sbe;

namespace {
std::shared_ptr<aeron::Subscription> findSubscription(
    const std::shared_ptr<aeron::Aeron>& aeron, std::int64_t id) {
  std::shared_ptr<aeron::Subscription> subscription;
  for (int attempt = 0; attempt < 200 && !subscription; ++attempt) {
    subscription = aeron->findSubscription(id);
    if (!subscription) std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  if (!subscription) throw std::runtime_error("Aeron subscription did not become available");
  return subscription;
}

}  // namespace

AeronCommandSubscriber::AeronCommandSubscriber(
    std::string aeronDir, std::string channel, int orderStream, int cancelStream) {
  aeron::Context context;
  context.aeronDir(aeronDir);
  aeron_ = aeron::Aeron::connect(context);
  orderSubscription_ = findSubscription(aeron_, aeron_->addSubscription(channel, orderStream));
  cancelSubscription_ = findSubscription(aeron_, aeron_->addSubscription(channel, cancelStream));
}

AeronCommandSubscriber::~AeronCommandSubscriber() = default;

void AeronCommandSubscriber::poll(
    int fragmentLimit, const OrderHandler& onOrder, const CancelHandler& onCancel) {
  orderSubscription_->poll([&](aeron::AtomicBuffer& buffer, aeron::util::index_t offset,
                               aeron::util::index_t length, auto&) {
    auto* data = reinterpret_cast<char*>(buffer.buffer()) + offset;
    sbe::MessageHeader header;
    header.wrap(data, 0, 0, static_cast<std::uint64_t>(length));
    if (header.templateId() != sbe::OrderRequest::sbeTemplateId()) return;
    sbe::OrderRequest frame;
    frame.wrapForDecode(data, sbe::MessageHeader::encodedLength(), header.blockLength(),
                        header.version(), static_cast<std::uint64_t>(length));
    OrderRequest request;
    request.orderRef = frame.orderRef();
    request.instrumentId = frame.getInstrumentIdAsString();
    request.exchange = frame.getExchangeAsString();
    request.side = static_cast<Side>(frame.side());
    request.offsetFlag = static_cast<Offset>(frame.offsetFlag());
    request.orderType = static_cast<OrderType>(frame.orderType());
    request.timeInForce = static_cast<TimeInForce>(frame.timeInForce());
    request.price = frame.price();
    request.qty = frame.qty();
    request.accountId = frame.getAccountIdAsString();
    request.venueId = frame.getVenueIdAsString();
    request.requestId = frame.getRequestIdAsString();
    request.sourceSessionId = frame.sourceSessionId();
    request.sourcePublicationEpoch = frame.sourcePublicationEpoch();
    onOrder(request);
  }, fragmentLimit);

  cancelSubscription_->poll([&](aeron::AtomicBuffer& buffer, aeron::util::index_t offset,
                                 aeron::util::index_t length, auto&) {
    auto* data = reinterpret_cast<char*>(buffer.buffer()) + offset;
    sbe::MessageHeader header;
    header.wrap(data, 0, 0, static_cast<std::uint64_t>(length));
    if (header.templateId() != sbe::CancelRequest::sbeTemplateId()) return;
    sbe::CancelRequest frame;
    frame.wrapForDecode(data, sbe::MessageHeader::encodedLength(), header.blockLength(),
                        header.version(), static_cast<std::uint64_t>(length));
    CancelRequest request;
    request.orderRef = frame.orderRef();
    request.instrumentId = frame.getInstrumentIdAsString();
    request.exchange = frame.getExchangeAsString();
    request.accountId = frame.getAccountIdAsString();
    request.requestId = frame.getRequestIdAsString();
    request.venueId = frame.getVenueIdAsString();
    request.sourceSessionId = frame.sourceSessionId();
    request.sourcePublicationEpoch = frame.sourcePublicationEpoch();
    onCancel(request);
  }, fragmentLimit);
}

}  // namespace ors::ctp
