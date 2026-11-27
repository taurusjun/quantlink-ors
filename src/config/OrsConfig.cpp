#include "config/OrsConfig.h"

#include <cstdlib>
#include <filesystem>
#include <stdexcept>

#include "ors/base/ConfigFile.h"

namespace ors {
namespace {

int readInt(const base::ConfigFile& file, const char* key, int fallback) {
  if (!file.contains(key)) {
    return fallback;
  }
  return std::stoi(file.get(key));
}

std::string read(const base::ConfigFile& file, const char* key,
                 const char* fallback = "") {
  return file.contains(key) ? file.get(key) : fallback;
}

}  // namespace

OrsConfig OrsConfig::load(const std::string& path) {
  const base::ConfigFile file = base::ConfigFile::load(path, "ors");

  OrsConfig config;
  config.brokerId = file.require("BROKER_ID");
  config.investorId = file.require("INVESTOR_ID");
  config.password = file.require("PASSWORD");
  config.tradeFront = file.require("TRADE_FRONT");
  config.mdFront = file.require("MD_FRONT");
  config.flowDir = file.require("FLOW_DIR");
  config.traderFlowDir = read(file, "TRADER_FLOW_DIR", config.flowDir.c_str());
  config.mdFlowDir = read(file, "MD_FLOW_DIR", config.flowDir.c_str());
  config.accountId = file.require("ACCOUNT_ID");
  config.instrument = read(file, "INSTRUMENT");
  const std::string defaultHighWater =
      (std::filesystem::path(config.flowDir).parent_path() /
       "client-id.highwater")
          .string();
  config.clientIdHighWaterFile =
      read(file, "CLIENT_ID_HIGHWATER", defaultHighWater.c_str());

  config.authCode = read(file, "AUTH_CODE");
  config.appId = read(file, "APP_ID");
  config.logPath = read(file, "LOG_FILE", "ors.log");
  config.aeronDir = read(file, "AERON_DIR");
  config.aeronChannel = read(file, "AERON_CHANNEL", "aeron:ipc");
  config.venueId = read(file, "VENUE_ID", "CTP");
  config.orsId = read(file, "ORS_ID", config.accountId.c_str());

  config.requestStream = readInt(file, "REQUEST_STREAM", 1);
  config.responseStream = readInt(file, "RESPONSE_STREAM", 2);
  config.accountStream = readInt(file, "ACCOUNT_STREAM", 3);
  config.marketDataStream = readInt(file, "MARKET_DATA_STREAM", 4);
  config.sessionStream = readInt(file, "SESSION_STREAM", 5);
  config.controlStream = readInt(file, "CONTROL_STREAM", 6);
  config.accountQueryTimeoutMs = readInt(file, "ACCOUNT_QUERY_TIMEOUT_MS", 5000);
  config.rmsConfigFile = read(file, "RMS_CONFIG_FILE");
  config.rmsFirmwideConfigFile = read(file, "RMS_FIRMWIDE_CONFIG_FILE");
  return config;
}

}  // namespace ors
