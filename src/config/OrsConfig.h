#pragma once

#include <string>

namespace ors {

struct OrsConfig {
  std::string brokerId;
  std::string investorId;
  std::string password;
  std::string authCode;
  std::string appId;
  std::string tradeFront;
  std::string mdFront;
  std::string flowDir;
  std::string traderFlowDir;
  std::string mdFlowDir;
  std::string logPath;
  std::string aeronDir;
  std::string aeronChannel{"aeron:ipc"};
  std::string accountId;
  std::string instrument;
  std::string clientIdHighWaterFile;
  std::string venueId;
  std::string orsId;
  int requestStream{1};
  int responseStream{2};
  int accountStream{3};
  int marketDataStream{4};
  int sessionStream{5};
  int controlStream{6};
  int accountQueryTimeoutMs{5000};
  std::string rmsConfigFile;
  std::string rmsFirmwideConfigFile;

  static OrsConfig load(const std::string& path);
};

}  // namespace ors
