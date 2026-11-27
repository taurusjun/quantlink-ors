#pragma once

#include <cstddef>
#include <cstdint>

#include "com_quantlink_contract_sbe/AccountQuery.h"
#include "com_quantlink_contract_sbe/AccountSnapshot.h"
#include "com_quantlink_contract_sbe/ClientRegister.h"
#include "com_quantlink_contract_sbe/ClientRegisterResponse.h"
#include "com_quantlink_contract_sbe/MarketUpdate.h"
#include "com_quantlink_contract_sbe/OrsRequest.h"
#include "com_quantlink_contract_sbe/OrsResponse.h"
#include "domain/OrsTypes.h"
#include "marketupdateNew.h"
#include "orderresponse.h"

namespace ors {

::ors::infra::RequestMsg decodeRequest(
    com::quantlink::contract::sbe::OrsRequest& decoder);
std::size_t encodeResponse(const ::ors::infra::ResponseMsg& response,
                           char* destination, std::size_t capacity);
std::size_t encodeMarketUpdate(const ::ors::md::MarketUpdateNew& update,
                               char* destination, std::size_t capacity);
AccountQueryRequest decodeAccountQuery(
    com::quantlink::contract::sbe::AccountQuery& decoder);
std::size_t encodeAccountSnapshot(const AccountSnapshot& snapshot,
                                  char* destination, std::size_t capacity);
ClientRegisterRequest decodeClientRegister(
    com::quantlink::contract::sbe::ClientRegister& decoder);
std::size_t encodeClientRegisterResponse(const ClientRegisterResult& result,
                                         char* destination,
                                         std::size_t capacity);

}  // namespace ors
