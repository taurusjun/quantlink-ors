// CtpConfig unit test: required keys, environment overrides for credentials,
// and log safety. Uses only CtpConfig, so it links without the CTP SDK.

#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "ors/ctp/CtpConfig.h"

namespace {

using ors::ctp::CtpConfig;

int failures = 0;

void check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

std::string writeConfig(const std::filesystem::path& path, const std::string& contents) {
  std::ofstream out(path, std::ios::trunc);
  out << contents;
  out.close();
  return path.string();
}

CtpConfig::Env envFrom(std::map<std::string, std::string> values) {
  return [values](std::string_view name) -> std::optional<std::string> {
    const auto found = values.find(std::string(name));
    if (found == values.end()) {
      return std::nullopt;
    }
    return found->second;
  };
}

template <typename Callable>
bool throwsContaining(Callable callable, const std::string& needle) {
  try {
    callable();
  } catch (const std::runtime_error& error) {
    return std::string(error.what()).find(needle) != std::string::npos;
  }
  return false;
}

}  // namespace

int main() {
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() / "ors_ctp_config_test";
  std::error_code ignored;
  std::filesystem::remove_all(dir, ignored);
  std::filesystem::create_directories(dir);

  const std::string full = writeConfig(dir / "full.cfg",
                                       "BROKER_ID = 9999\n"
                                       "INVESTOR_ID = 900001\n"
                                       "PASSWORD = file-password\n"
                                       "APP_ID = file-app\n"
                                       "AUTH_CODE = file-auth\n"
                                       "TRADE_FRONT = tcp://127.0.0.1:30001\n"
                                       "MD_FRONT = tcp://127.0.0.1:30011\n"
                                       "TRADER_FLOW_PATH = state/test/trader\n");

  // Required keys fail with the key name, never the value.
  const std::string noInvestor = writeConfig(dir / "no_investor.cfg",
                                             "BROKER_ID = 9999\n"
                                             "PASSWORD = secret-value\n"
                                             "APP_ID = app\n"
                                             "AUTH_CODE = auth\n"
                                             "TRADE_FRONT = tcp://127.0.0.1:30001\n");
  check(throwsContaining([&] { CtpConfig::load(noInvestor, envFrom({})); }, "INVESTOR_ID"),
        "missing INVESTOR_ID throws naming INVESTOR_ID");

  const std::string noPassword = writeConfig(dir / "no_password.cfg",
                                             "BROKER_ID = 9999\n"
                                             "INVESTOR_ID = 900001\n"
                                             "APP_ID = app\n"
                                             "AUTH_CODE = auth\n"
                                             "TRADE_FRONT = tcp://127.0.0.1:30001\n");
  const bool passwordThrows = throwsContaining(
      [&] { CtpConfig::load(noPassword, envFrom({})); }, "PASSWORD");
  check(passwordThrows, "missing PASSWORD throws naming PASSWORD");
  const bool noValueLeak = !throwsContaining(
      [&] { CtpConfig::load(noInvestor, envFrom({})); }, "secret-value");
  check(noValueLeak, "missing-key error does not echo other values");

  check(throwsContaining([&] { CtpConfig::load(dir / "absent.cfg", envFrom({})); }, "cannot open"),
        "missing file throws");

  const std::string noAuthCode = writeConfig(dir / "no_auth.cfg",
                                             "BROKER_ID = 9999\n"
                                             "INVESTOR_ID = 900001\n"
                                             "PASSWORD = secret-value\n"
                                             "APP_ID = app\n"
                                             "TRADE_FRONT = tcp://127.0.0.1:30001\n");
  check(throwsContaining([&] { CtpConfig::load(noAuthCode, envFrom({})); }, "AUTH_CODE"),
        "missing AUTH_CODE throws naming AUTH_CODE");

  // The file alone is enough when it carries the credentials.
  const CtpConfig fromFile = CtpConfig::load(full, envFrom({}));
  check(fromFile.password == "file-password", "PASSWORD read from file");
  check(fromFile.brokerId == "9999", "BROKER_ID read from file");
  check(fromFile.investorId == "900001", "INVESTOR_ID read from file");
  check(fromFile.tradeFront == "tcp://127.0.0.1:30001", "TRADE_FRONT read from file");
  check(fromFile.traderFlowPath == "state/test/trader", "TRADER_FLOW_PATH read from file");

  // Environment overrides each credential; non-credential keys are untouched.
  const CtpConfig fromEnv = CtpConfig::load(
      full, envFrom({{"CTP_PASSWORD", "env-password"},
                     {"CTP_APP_ID", "env-app"},
                     {"CTP_AUTH_CODE", "env-auth"}}));
  check(fromEnv.password == "env-password", "CTP_PASSWORD overrides file");
  check(fromEnv.appId == "env-app", "CTP_APP_ID overrides file");
  check(fromEnv.authCode == "env-auth", "CTP_AUTH_CODE overrides file");
  check(fromEnv.investorId == "900001", "env override leaves INVESTOR_ID alone");

  // A present-but-empty variable must not clobber the file value.
  const CtpConfig blankEnv =
      CtpConfig::load(full, envFrom({{"CTP_PASSWORD", ""}, {"CTP_AUTH_CODE", ""}}));
  check(blankEnv.password == "file-password", "empty env var does not override password");
  check(blankEnv.authCode == "file-auth", "empty env var does not override auth code");

  // Credentials may live only in the environment; the file then omits the keys.
  const std::string envOnlyFile = writeConfig(dir / "env_only.cfg",
                                              "BROKER_ID = 9999\n"
                                              "INVESTOR_ID = 900001\n"
                                              "TRADE_FRONT = tcp://127.0.0.1:30001\n");
  try {
    const CtpConfig envOnly =
        CtpConfig::load(envOnlyFile, envFrom({{"CTP_PASSWORD", "env-password"},
                                              {"CTP_APP_ID", "env-app"},
                                              {"CTP_AUTH_CODE", "env-auth"}}));
    check(envOnly.password == "env-password" && envOnly.appId == "env-app" &&
              envOnly.authCode == "env-auth",
          "credentials can come only from the environment");
  } catch (const std::exception& error) {
    check(false, std::string("env-only credentials should load: ") + error.what());
  }

  // summary() is the loggable form: no credential may appear in it.
  const std::string summary = fromEnv.summary();
  const bool leaks = summary.find("env-password") != std::string::npos ||
                     summary.find("env-auth") != std::string::npos ||
                     summary.find("file-password") != std::string::npos;
  check(!leaks, "summary does not contain password or auth code");

  if (failures != 0) {
    std::cerr << failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "ctp_config: required keys, env overrides, and redacted summary\n";
  return 0;
}
