#include "execution/OrderService.h"

#include "session/ORSServer.h"

#include <cstdio>
#include <cstring>

#include "infra/LogFacade.h"
#include "infra/Time.h"

namespace ors {

OrderService::OrderService(OrderStore& orders, PushResponse push)
    : orders_(orders), push_(std::move(push)) {}

bool OrderService::crossCheck(const ::ors::infra::RequestMsg& request) {
  std::lock_guard<std::mutex> lock(orders_.localMutex());
  for (const auto& entry : orders_.local()) {
    const OrderInfo& open = entry.second;
    if (request.Transaction_Type == open.Transaction_Type) {
      continue;
    }
    if (((open.Transaction_Type == 'B' && request.Price <= open.Price) ||
         (open.Transaction_Type == 'S' &&
          (request.Price >= open.Price || request.Price == 0))) &&
        std::strcmp(request.Contract_Description.Symbol, open.ScripCode) == 0) {
      log::info("ors.order.cross_reject",
                {{"open_order_id", static_cast<std::uint64_t>(entry.first)},
                 {"new_order_id", static_cast<std::uint64_t>(request.OrderID)}});
      return true;
    }
  }
  return false;
}

void OrderService::setNextOrderRef(int value) {
  nextOrderRef_ = value < 0 ? 0 : value;
}

std::string OrderService::takeOrderRef() {
  char orderRef[16]{};
  std::snprintf(orderRef, sizeof(orderRef), "%012d", nextOrderRef_++);
  return orderRef;
}

void OrderService::reject(const ::ors::infra::RequestMsg& request,
                          const char* reason) {
  ::ors::infra::ResponseMsg response{};
  if (request.Request_Type == ::ors::infra::NEWORDER) {
    response.Response_Type = ::ors::infra::ORS_REJECT;
    response.ErrorCode = kCrossRejectCode;
  } else if (request.Request_Type == ::ors::infra::MODIFYORDER) {
    response.Response_Type = ::ors::infra::MODIFY_ORDER_REJECT;
  } else if (request.Request_Type == ::ors::infra::CANCELORDER) {
    response.Response_Type = ::ors::infra::CANCEL_ORDER_REJECT;
  }
  response.OrderID = request.OrderID;
  std::strncpy(response.AccountID, request.AccountID,
               sizeof(response.AccountID) - 1);
  log::warn("ors.order.rejected",
            {{"order_id", static_cast<std::uint64_t>(request.OrderID)},
             {"reason", reason == nullptr ? "" : reason}});
  if (push_) {
    push_(&response);
  }
}


void OrderService::sendNewOrder(ORSServer& server, ::ors::infra::RequestMsg& requestRef)
{
    server.tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "NewOrder ------> " << server.tempTimeStamp << "|" << __FUNCTION__ << " start OrderRef" << requestRef.OrderID << std::endl;
    int iResult;
    CThostFtdcInputOrderField req;
    ::ors::OrderInfo ordinfo;

    uint64_t rmstime, ordercrosscheck, apisend;
    uint64_t curtime = ::ors::nowNanos();
    memset(&req, 0, sizeof(req));

    if (!server.rms_.impl().executeNewOrderChecks(requestRef))
    {
        ORS_LOG(INFO) << "NewOrder ------> "
                   << "NewOrder check failed. orderID:" << requestRef.OrderID << std::endl;
        return;
    }
    rmstime = ::ors::nowNanos();

    ordercrosscheck = ::ors::nowNanos();

    if (requestRef.OrdType == MARKET)
    {

        // defaults:
        req.OrderPriceType = THOST_FTDC_OPT_AnyPrice;
        req.LimitPrice = 0;
        req.TimeCondition = THOST_FTDC_TC_IOC;
        req.VolumeTotalOriginal = requestRef.Quantity;
        req.VolumeCondition = THOST_FTDC_VC_CV;
        req.MinVolume = requestRef.Quantity;
        if ((requestRef.Duration == ::ors::infra::FOK) || (requestRef.Duration == ::ors::infra::IOC))
        {
            if ((requestRef.Exchange_Type == CHINA_CFFEX) || (requestRef.Exchange_Type == CHINA_ZCE))
            {
                server.RejectOrder(&requestRef, "Order type not supported in this market");
                return;
            }
            req.OrderPriceType = THOST_FTDC_OPT_AnyPrice;
            req.LimitPrice = 0;
            req.TimeCondition = THOST_FTDC_TC_IOC;
            req.VolumeTotalOriginal = requestRef.Quantity;
            req.VolumeCondition = THOST_FTDC_VC_CV;
            req.MinVolume = requestRef.Quantity;
        }
        else if (requestRef.Duration == ::ors::infra::FAK)
        {
            if ((requestRef.Exchange_Type == CHINA_CFFEX) || (requestRef.Exchange_Type == CHINA_ZCE))
            {
                server.RejectOrder(&requestRef, "Order type not supported in this market");
                return;
            }
            req.VolumeCondition = THOST_FTDC_VC_AV;
            req.MinVolume = 1;
        }
        else
        {
            // TODO : check price type
            req.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
            req.LimitPrice = 0;
            req.TimeCondition = THOST_FTDC_TC_GFD;
            req.VolumeTotalOriginal = requestRef.Quantity;
            req.VolumeCondition = THOST_FTDC_VC_AV;
            req.MinVolume = 1;
        }
    }
    else if (requestRef.OrdType == LIMIT)
    {
        // defaults
        req.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
        req.LimitPrice = requestRef.Price;
        req.TimeCondition = THOST_FTDC_TC_IOC;
        req.VolumeTotalOriginal = requestRef.Quantity;
        req.VolumeCondition = THOST_FTDC_VC_CV;
        req.MinVolume = requestRef.Quantity;

        if ((requestRef.Duration == ::ors::infra::FOK) || (requestRef.Duration == ::ors::infra::IOC))
        {
            if ((requestRef.Exchange_Type == CHINA_CFFEX) || (requestRef.Exchange_Type == CHINA_ZCE))
            {
                server.RejectOrder(&requestRef, "Order type not supported in this market");
                return;
            }

            req.VolumeCondition = THOST_FTDC_VC_CV;
            req.MinVolume = requestRef.Quantity;
        }
        else if (requestRef.Duration == ::ors::infra::FAK)
        {
            // if(!strcmp(symbol->second.ExchangeID, "CFFEX") || !strcmp(symbol->second.ExchangeID, "ZCE"))
            if ((requestRef.Exchange_Type == CHINA_CFFEX) || (requestRef.Exchange_Type == CHINA_ZCE))
            {
                server.RejectOrder(&requestRef, "Order type not supported in this market");
                return;
            }
            req.VolumeCondition = THOST_FTDC_VC_AV;
            req.MinVolume = 1;
        }
        else
        {
            req.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
            req.LimitPrice = requestRef.Price;
            req.TimeCondition = THOST_FTDC_TC_GFD;
            req.VolumeTotalOriginal = requestRef.Quantity;
            req.VolumeCondition = THOST_FTDC_VC_AV;
            req.MinVolume = 1;
        }
    }
    else
    {
        req.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
        req.LimitPrice = requestRef.Price;
        req.TimeCondition = THOST_FTDC_TC_GFD;
        req.VolumeTotalOriginal = requestRef.Quantity;
        req.VolumeCondition = THOST_FTDC_VC_AV;
        req.MinVolume = 1;
    }

    strcpy(req.BrokerID, server.m_BROKER_ID.c_str());
    strcpy(req.InvestorID, server.m_INVESTOR_ID.c_str());
    strcpy(req.InstrumentID, requestRef.Contract_Description.Symbol);
    const std::string orderRef = takeOrderRef();
    strcpy(req.OrderRef, orderRef.c_str());

    if (requestRef.Transaction_Type == 'B')
    {
        req.Direction = THOST_FTDC_D_Buy;
    }
    else if (requestRef.Transaction_Type == 'S')
    {
        req.Direction = THOST_FTDC_D_Sell;
    }
    else
    {
        server.RejectOrder(&requestRef, "New order Send Error");
        return;
    }
    /*
     switch(requestRef.getDirection())
     {
     case CLOSE:                 req.CombOffsetFlag[0] = THOST_FTDC_OF_Close; req.CombOffsetFlag[1] = '\0'; 	break;
     case CLOSE_INTRADAY:
     if(requestRef.Exchange_Type == CHINA_SHFE)
     {
     req.CombOffsetFlag[0] = THOST_FTDC_OF_CloseToday; req.CombOffsetFlag[1] = '\0';
     }
     else
     {
     req.CombOffsetFlag[0] = THOST_FTDC_OF_Close; req.CombOffsetFlag[1] = '\0';
     }
     break;
     case OPEN:                  req.CombOffsetFlag[0] = THOST_FTDC_OF_Open;	req.CombOffsetFlag[1] = '\0'; break;
     default :
     std::cout << "Reject order as direction not set" << std::endl;
     server.RejectOrder( request, "New order reject\n");
     return;
     }
     */

    char flag;
    int iOrderType = 0;
    server.SetCombOffsetFlag(req, &requestRef, flag, iOrderType);

    req.CombOffsetFlag[0] = flag;
    req.CombOffsetFlag[1] = '\0';
    req.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
    req.CombHedgeFlag[1] = '\0';
    req.ContingentCondition = THOST_FTDC_CC_Immediately;
    req.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
    req.IsAutoSuspend = 0;
    req.UserForceClose = 0;
    req.RequestID = server.m_iRequestID + 1;
    apisend = ::ors::nowNanos();
    iResult = server.m_pUserTradeApi->ReqOrderInsert(&req, ++server.m_iRequestID);
    if (iResult != 0)
    {
        server.RejectOrder(&requestRef, "New order Send Error");
        /*
         iNextOrderRef--;
         sprintf(ORDER_REF, "%d", iNextOrderRef);
         */
        return;
    }

    uint64_t endtime = ::ors::nowNanos();

    // maintain map here
    ordinfo.requestType = requestRef.Request_Type;
    ordinfo.orderSent = 1;
    ordinfo.localID = requestRef.OrderID;
    strcpy(ordinfo.exchID, orderRef.c_str());
    strcpy(ordinfo.ClientID, requestRef.AccountID);
    strcpy(ordinfo.ScripCode, requestRef.Contract_Description.Symbol);
    ordinfo.Transaction_Type = requestRef.Transaction_Type;
    ordinfo.Price = requestRef.Price;
    ordinfo.Qty = requestRef.Quantity;
    ordinfo.ModQtySent = 0;
    ordinfo.oldPrice = 0;
    ordinfo.oldQty = 0;
    ordinfo.openQty = requestRef.Quantity;
    ordinfo.exchange = requestRef.Exchange_Type;
    ordinfo.sendTime = endtime;
    ordinfo.frontId = server.FRONT_ID;
    ordinfo.sessionId = server.SESSION_ID;
    ordinfo.qtyType = iOrderType;

    orders_.localMutex().lock();
    orders_.local()[requestRef.OrderID] = ordinfo;
    orders_.localMutex().unlock();

    orders_.exchangeMutex().lock();
    orders_.exchange()[std::string(req.OrderRef)] = requestRef.OrderID;
    orders_.exchangeMutex().unlock();

    server.tempTimeStamp = ::ors::nowNanos();
    //  ORS_LOG(INFO) << "--->>> "<< server.tempTimeStamp << "|" << "New Order placed | " << requestRef.Contract_Description.Symbol << " " << requestRef.OrderID  << " " << ORDER_REF << " " << requestRef.Price << " " << requestRef.Quantity << std::endl ;
    // ORS_LOG(INFO) << "------> "<< server.tempTimeStamp << "|" << __FUNCTION__  << " end OrderRef"<< requestRef.OrderID << std::endl;
}

void OrderService::sendCancelOrder(ORSServer& server, ::ors::infra::RequestMsg& requestRef)
{
    server.tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "------> " << server.tempTimeStamp << "|" << __FUNCTION__ << " start OrderRef" << requestRef.OrderID << std::endl;
    CThostFtdcInputOrderActionField req;
    ::ors::OrderInfo ordinfo;
    int iResult;
    uint64_t rmstime, apisend;
    uint64_t curtime = ::ors::nowNanos();

    memset(&req, 0, sizeof(req));

    orders_.localMutex().lock();
    if (orders_.local().find(requestRef.OrderID) == orders_.local().end())
    {
        orders_.localMutex().unlock();
        server.RejectOrder(&requestRef, "New not found for Cancel request");
        return;
    }
    ordinfo = orders_.local()[requestRef.OrderID];
    orders_.localMutex().unlock();

    if (!server.rms_.impl().executeCancelOrderChecks(requestRef))
    {
        return;
    }
    rmstime = ::ors::nowNanos();

    strcpy(req.BrokerID, server.m_BROKER_ID.c_str());
    // strcpy(req.UserID, server.m_BROKER_ID.c_str());
    strcpy(req.InvestorID, server.m_INVESTOR_ID.c_str());
    strcpy(req.InstrumentID, requestRef.Contract_Description.Symbol);
    strcpy(req.OrderRef, ordinfo.exchID);
    req.FrontID = server.FRONT_ID;
    req.SessionID = server.SESSION_ID;
    req.ActionFlag = THOST_FTDC_AF_Delete;
    req.RequestID = server.m_iRequestID + 1;
    req.LimitPrice = requestRef.Price;
    req.VolumeChange = requestRef.Quantity;
    switch (requestRef.Exchange_Type)
    {
    case CHINA_SHFE:
        strcpy(req.ExchangeID, "SHFE");
        break;
    case CHINA_CFFEX:
        strcpy(req.ExchangeID, "CFFEX");
        break;
    case CHINA_ZCE:
        strcpy(req.ExchangeID, "CZCE");
        break;
    case CHINA_DCE:
        strcpy(req.ExchangeID, "DCE");
        break;
    }
    strcpy(req.OrderSysID, ordinfo.OrderSysID);

    apisend = ::ors::nowNanos();
    ORS_LOG(INFO) << "After API call 4\n";
    iResult = server.m_pUserTradeApi->ReqOrderAction(&req, ++server.m_iRequestID);
    ORS_LOG(INFO) << "After API call 4\n";
    if (iResult != 0)
    {
        server.RejectOrder(&requestRef, "Cancel order send error");
        return;
    }
    uint64_t endtime = ::ors::nowNanos();

    ordinfo.ModQtySent = 0;
    ordinfo.requestType = CANCELORDER; // requestRef.Request_Type;
    ordinfo.sendTime = endtime;

    orders_.localMutex().lock();
    orders_.local()[requestRef.OrderID] = ordinfo;
    orders_.localMutex().unlock();

    server.tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "--->>> " << server.tempTimeStamp << "|"
               << "Cancel order sent | " << ordinfo.localID << " | " << ordinfo.exchID << '|' << requestRef.Price << '|' << requestRef.Quantity << std::endl;
    ORS_LOG(INFO) << "------> " << server.tempTimeStamp << "|" << __FUNCTION__ << " end OrderRef" << requestRef.OrderID << std::endl;

}

}  // namespace ors
