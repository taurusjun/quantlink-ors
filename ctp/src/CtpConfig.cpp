#include "ors/ctp/CtpConfig.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <unordered_map>

namespace ors::ctp {
namespace {

std::string trim(std::string_view value) {
  auto isSpace = [](char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; };
  while (!value.empty() && isSpace(value.front())) {
    value.remove_prefix(1);
  }
  while (!value.empty() && isSpace(value.back())) {
    value.remove_suffix(1);
  }
  return std::string(value);
}

std::string get(const std::unordered_map<std::string, std::string>& values,
                const std::string& key) {
  const auto found = values.find(key);
  return found == values.end() ? std::string() : found->second;
}

std::string require(const std::unordered_map<std::string, std::string>& values,
                    const std::string& key, const char* envName = nullptr) {
  const std::string value = get(values, key);
  if (!value.empty()) {
    return value;
  }
  std::string message = "ctp config: " + key + " is required";
  if (envName != nullptr) {
    message += std::string(" (file or env ") + envName + ")";
  }
  throw std::runtime_error(message);
}

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
  std::ifstream in(path);
  if (!in) {
    throw std::runtime_error("ctp config: cannot open " + path);
  }

  std::unordered_map<std::string, std::string> values;
  std::string line;
  while (std::getline(in, line)) {
    const auto hash = line.find('#');
    if (hash != std::string::npos) {
      line = line.substr(0, hash);
    }
    const auto equals = line.find('=');
    if (equals == std::string::npos) {
      continue;
    }
    const std::string key = trim(std::string_view(line).substr(0, equals));
    if (!key.empty()) {
      values[key] = trim(std::string_view(line).substr(equals + 1));
    }
  }

  // Environment wins for the credential keys; an empty variable is treated as
  // unset so an exported-but-blank value cannot silently wipe the file.
  const auto credential = [&](const std::string& key, const char* envName) {
    const auto value = env(envName);
    if (value.has_value() && !value->empty()) {
      return *value;
    }
    return require(values, key, envName);
  };

  CtpConfig config;
  config.brokerId = require(values, "BROKER_ID");
  config.investorId = require(values, "INVESTOR_ID");
  config.password = credential("PASSWORD", "CTP_PASSWORD");
  config.appId = credential("APP_ID", "CTP_APP_ID");
  config.authCode = credential("AUTH_CODE", "CTP_AUTH_CODE");
  // FRONT_ADDR_ORS / FRONT_ADDR_MD are the older quantlink key names.
  config.tradeFront = get(values, "TRADE_FRONT");
  if (config.tradeFront.empty()) {
    config.tradeFront = get(values, "FRONT_ADDR_ORS");
  }
  if (config.tradeFront.empty()) {
    throw std::runtime_error("ctp config: TRADE_FRONT is required");
  }
  config.mdFront = get(values, "MD_FRONT");
  if (config.mdFront.empty()) {
    config.mdFront = get(values, "FRONT_ADDR_MD");
  }
  const auto traderFlow = get(values, "TRADER_FLOW_PATH");
  if (!traderFlow.empty()) {
    config.traderFlowPath = traderFlow;
  }
  const auto mdFlow = get(values, "MD_FLOW_PATH");
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
