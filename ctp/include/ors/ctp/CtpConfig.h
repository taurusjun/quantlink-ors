#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace ors::ctp {

/**
 * CTP connection configuration.
 *
 * Credentials live outside the repository: the file may carry them, and the
 * environment variables CTP_PASSWORD / CTP_APP_ID / CTP_AUTH_CODE override
 * whatever the file says. Values are never echoed back; use summary() to log.
 */
struct CtpConfig {
  std::string brokerId;
  std::string investorId;
  std::string password;
  std::string appId;
  std::string authCode;
  std::string tradeFront;
  std::string mdFront;
  std::string traderFlowPath{"state/ctp/trader"};
  std::string mdFlowPath{"state/ctp/md"};

  /// Returns the variable's value, or nullopt when it is not set.
  using Env = std::function<std::optional<std::string>(std::string_view name)>;

  /// Reads a key=value file. Throws std::runtime_error naming the key on
  /// missing file or missing required value; never includes the value.
  static CtpConfig load(const std::string& path, const Env& env = processEnv());

  static Env processEnv();

  /// Redacted, safe to log.
  std::string summary() const;
};

}  // namespace ors::ctp
