#include "transport/AeronOrsTransport.h"

#include <string_view>

#include "com_quantlink_contract_sbe/MessageHeader.h"
#include "com_quantlink_contract_sbe/SessionEvent.h"
#include "contract/OrsCodec.h"

namespace ors {
namespace {

namespace sbe = com::quantlink::contract::sbe;

}  // namespace

AeronOrsTransport::AeronOrsTransport(OrsConfig config,
                                     FramePublisher& publisher,
                                     FrameSubscriber& subscriber)
    : config_(std::move(config)),
      publisher_(publisher),
      subscriber_(subscriber) {}

bool AeronOrsTransport::start(RequestHandler requestHandler,
                              AccountQueryHandler accountQueryHandler,
                              ClientRegisterHandler clientRegisterHandler) {
  requestHandler_ = std::move(requestHandler);
  accountQueryHandler_ = std::move(accountQueryHandler);
  clientRegisterHandler_ = std::move(clientRegisterHandler);
  return true;
}

bool AeronOrsTransport::sendResponse(
    const ::ors::infra::ResponseMsg& response) {
  const std::size_t length =
      encodeResponse(response, buffer_.data(), buffer_.size());
  return publish(config_.responseStream, length);
}

bool AeronOrsTransport::sendMarketData(
    const ::ors::md::MarketUpdateNew& update) {
  const std::size_t length =
      encodeMarketUpdate(update, buffer_.data(), buffer_.size());
  return publish(config_.marketDataStream, length);
}

bool AeronOrsTransport::sendSession(const SessionEvent& event) {
  // Session encoding is kept separate from the order codec; use the contract
  // SessionEvent encoder in the same way as the other messages.
  sbe::SessionEvent message;
  message.wrapAndApplyHeader(buffer_.data(), 0, buffer_.size())
      .putVenueId(std::string_view(event.venueId))
      .putAccountId(std::string_view(event.accountId))
      .putOrsId(std::string_view(event.orsId))
      .state(static_cast<sbe::SessionState::Value>(event.state.empty()
                                                       ? 0
                                                       : std::stoi(event.state)))
      .putReason(std::string_view(event.reason))
      .timestamp(event.timestamp);
  return publish(config_.sessionStream,
                 message.encodedLength() + sbe::MessageHeader::encodedLength());
}

bool AeronOrsTransport::sendAccount(const AccountSnapshot& snapshot) {
  const std::size_t length =
      encodeAccountSnapshot(snapshot, buffer_.data(), buffer_.size());
  return publish(config_.accountStream, length);
}

bool AeronOrsTransport::sendClientRegisterResponse(
    const ClientRegisterResult& result) {
  const std::size_t length =
      encodeClientRegisterResponse(result, buffer_.data(), buffer_.size());
  return publish(config_.controlStream, length);
}

void AeronOrsTransport::stop() {}

void AeronOrsTransport::poll(int fragmentLimit) {
  subscriber_.poll(fragmentLimit,
                   [this](int, const char* data, std::size_t length) {
                     sbe::MessageHeader header;
                     header.wrap(const_cast<char*>(data), 0, 0, length);
                     const std::uint64_t offset =
                         sbe::MessageHeader::encodedLength();

                     switch (header.templateId()) {
                     case sbe::OrsRequest::sbeTemplateId(): {
                       sbe::OrsRequest message;
                       message.wrapForDecode(const_cast<char*>(data), offset, header.blockLength(),
                                             header.version(), length);
                       if (requestHandler_) {
                         auto request = decodeRequest(message);
                         requestHandler_(&request);
                       }
                       break;
                     }
                     case sbe::AccountQuery::sbeTemplateId(): {
                       sbe::AccountQuery message;
                       message.wrapForDecode(const_cast<char*>(data), offset, header.blockLength(),
                                             header.version(), length);
                       if (accountQueryHandler_) {
                         accountQueryHandler_(decodeAccountQuery(message));
                       }
                       break;
                     }
                     case sbe::ClientRegister::sbeTemplateId(): {
                       sbe::ClientRegister message;
                       message.wrapForDecode(const_cast<char*>(data), offset, header.blockLength(),
                                             header.version(), length);
                       if (clientRegisterHandler_) {
                         clientRegisterHandler_(decodeClientRegister(message));
                       }
                       break;
                     }
                     default:
                       break;
                     }
                   });
}

bool AeronOrsTransport::publish(int streamId, std::size_t length) {
  if (length == 0) {
    return false;
  }
  return publisher_.publish(streamId, buffer_.data(), length);
}

}  // namespace ors
