#include "ors/ctp/CtpConfig.h"

#include <cstdlib>
#include <stdexcept>
#include <string>

#include "ors/ctp/ConfigFile.h"

namespace ors::ctp {
namespace {

std::string secret(const std::string& value) {
  return value.empty() ? "<empty>" : "***";
}

}  // namespace

CtpConfig::Env CtpConfig::processEnv() {
  return [](std::string_view name) -> std::optional<std::string> {
    const char* value = std::getenv(std::string(name).c_str());
    if (value == nullptr) {
      return std::nullopt;
    }
    return std::string(value);
  };
}

CtpConfig CtpConfig::load(const std::string& path, const Env& env) {
  const ConfigFile file = ConfigFile::load(path, "ctp config");

  // Environment wins for the credential keys; an empty variable is treated as
  // unset so an exported-but-blank value cannot silently wipe the file.
  const auto credential = [&](const std::string& key, const char* envName) {
    const auto value = env(envName);
    if (value.has_value() && !value->empty()) {
      return *value;
    }
    return file.require(key, envName);
  };

  CtpConfig config;
  config.brokerId = file.require("BROKER_ID");
  config.investorId = file.require("INVESTOR_ID");
  config.password = credential("PASSWORD", "CTP_PASSWORD");
  config.appId = credential("APP_ID", "CTP_APP_ID");
  config.authCode = credential("AUTH_CODE", "CTP_AUTH_CODE");
  // FRONT_ADDR_ORS / FRONT_ADDR_MD are the older quantlink key names.
  config.tradeFront = file.get("TRADE_FRONT");
  if (config.tradeFront.empty()) {
    config.tradeFront = file.get("FRONT_ADDR_ORS");
  }
  if (config.tradeFront.empty()) {
    throw std::runtime_error("ctp config: TRADE_FRONT is required");
  }
  config.mdFront = file.get("MD_FRONT");
  if (config.mdFront.empty()) {
    config.mdFront = file.get("FRONT_ADDR_MD");
  }
  const auto traderFlow = file.get("TRADER_FLOW_PATH");
  if (!traderFlow.empty()) {
    config.traderFlowPath = traderFlow;
  }
  const auto mdFlow = file.get("MD_FLOW_PATH");
  if (!mdFlow.empty()) {
    config.mdFlowPath = mdFlow;
  }

  return config;
}

std::string CtpConfig::summary() const {
  return "trader=" + tradeFront + " md=" + mdFront + " broker=" + brokerId +
         " investor=" + investorId + " password=" + secret(password) +
         " appId=" + secret(appId) + " authCode=" + secret(authCode) +
         " traderFlow=" + traderFlowPath;
}

}  // namespace ors::ctp
