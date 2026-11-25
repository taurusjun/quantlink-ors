// RuntimeConfig unit test: strict keys, stream layout and optional values.
// Links only ors_ctp_config, so it runs without CTP or Aeron.

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include "ors/ctp/RuntimeConfig.h"

namespace {

using ors::ctp::ContractRuntimeConfig;

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

template <typename Callable>
bool throwsContaining(Callable callable, const std::string& needle) {
  try {
    callable();
  } catch (const std::runtime_error& error) {
    return std::string(error.what()).find(needle) != std::string::npos;
  }
  return false;
}

const char* kFull = R"(
# runtime
AERON_DIR = /var/lib/quantlink/aeron
VENUE_ID = SIMNOW
ORS_ID = ors-test
ENVIRONMENT = SHADOW
EXCHANGE = SHFE
INSTRUMENTS = au2612, ag2612
STREAM_BASE = 9000
SOURCE_PUBLICATION_EPOCH = 7
SOURCE_SESSION_ID = 42
QUERY_INTERVAL_SECONDS = 2
)";

}  // namespace

int main() {
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() / "ors_runtime_config_test";
  std::error_code ignored;
  std::filesystem::remove_all(dir, ignored);
  std::filesystem::create_directories(dir);

  const std::string full = writeConfig(dir / "full.cfg", kFull);
  const ContractRuntimeConfig config = ContractRuntimeConfig::load(full);
  check(config.aeronDir == "/var/lib/quantlink/aeron", "AERON_DIR is read");
  check(config.venueId == "SIMNOW", "VENUE_ID is read");
  check(config.orsId == "ors-test", "ORS_ID is read");
  check(config.environment == "SHADOW", "ENVIRONMENT is read");
  check(config.exchange == "SHFE", "EXCHANGE is read");
  check(config.instruments.size() == 2 && config.instruments[0] == "au2612" &&
            config.instruments[1] == "ag2612",
        "INSTRUMENTS is split");
  check(config.sourcePublicationEpoch == 7, "SOURCE_PUBLICATION_EPOCH is read");
  check(config.sourceSessionId == 42, "SOURCE_SESSION_ID is read");
  check(config.queryIntervalSeconds == 2, "QUERY_INTERVAL_SECONDS is read");
  check(config.orderStream == 9002, "order stream is base+2");
  check(config.cancelStream == 9003, "cancel stream is base+3");
  check(config.streams.marketData == 9001, "market-data stream is base+1");
  check(config.streams.execution == 9004, "execution stream is base+4");
  check(config.streams.position == 9005, "position stream is base+5");
  check(config.streams.account == 9006, "account stream is base+6");
  check(config.streams.session == 9007, "session stream is base+7");

  const std::string noVenue =
      writeConfig(dir / "no_venue.cfg",
                  "AERON_DIR = /tmp/aeron\n"
                  "ORS_ID = ors\n"
                  "ENVIRONMENT = SHADOW\n"
                  "EXCHANGE = SHFE\n"
                  "INSTRUMENTS = au2612\n"
                  "STREAM_BASE = 9000\n"
                  "SOURCE_PUBLICATION_EPOCH = 1\n");
  check(throwsContaining([&] { ContractRuntimeConfig::load(noVenue); }, "VENUE_ID"),
        "missing VENUE_ID names the key");

  const std::string noInstruments =
      writeConfig(dir / "no_instruments.cfg",
                  "AERON_DIR = /tmp/aeron\n"
                  "VENUE_ID = SIMNOW\n"
                  "ORS_ID = ors\n"
                  "ENVIRONMENT = SHADOW\n"
                  "EXCHANGE = SHFE\n"
                  "INSTRUMENTS = ,  \n"
                  "STREAM_BASE = 9000\n"
                  "SOURCE_PUBLICATION_EPOCH = 1\n");
  check(throwsContaining([&] { ContractRuntimeConfig::load(noInstruments); }, "INSTRUMENTS"),
        "empty INSTRUMENTS is rejected");

  const std::string badStream =
      writeConfig(dir / "bad_stream.cfg",
                  "AERON_DIR = /tmp/aeron\n"
                  "VENUE_ID = SIMNOW\n"
                  "ORS_ID = ors\n"
                  "ENVIRONMENT = SHADOW\n"
                  "EXCHANGE = SHFE\n"
                  "INSTRUMENTS = au2612\n"
                  "STREAM_BASE = nope\n"
                  "SOURCE_PUBLICATION_EPOCH = 1\n");
  check(throwsContaining([&] { ContractRuntimeConfig::load(badStream); }, "STREAM_BASE"),
        "a non-numeric STREAM_BASE names the key");

  const std::string negativeEpoch =
      writeConfig(dir / "negative_epoch.cfg",
                  "AERON_DIR = /tmp/aeron\n"
                  "VENUE_ID = SIMNOW\n"
                  "ORS_ID = ors\n"
                  "ENVIRONMENT = SHADOW\n"
                  "EXCHANGE = SHFE\n"
                  "INSTRUMENTS = au2612\n"
                  "STREAM_BASE = 9000\n"
                  "SOURCE_PUBLICATION_EPOCH = -1\n");
  check(throwsContaining([&] { ContractRuntimeConfig::load(negativeEpoch); },
                         "SOURCE_PUBLICATION_EPOCH"),
        "a negative epoch is rejected as unsigned");

  const std::string hugeStream =
      writeConfig(dir / "huge_stream.cfg",
                  "AERON_DIR = /tmp/aeron\n"
                  "VENUE_ID = SIMNOW\n"
                  "ORS_ID = ors\n"
                  "ENVIRONMENT = SHADOW\n"
                  "EXCHANGE = SHFE\n"
                  "INSTRUMENTS = au2612\n"
                  "STREAM_BASE = 2147483647\n"
                  "SOURCE_PUBLICATION_EPOCH = 1\n");
  check(throwsContaining([&] { ContractRuntimeConfig::load(hugeStream); }, "too large"),
        "a STREAM_BASE with no room for +7 is rejected");

  const std::string optional =
      writeConfig(dir / "optional.cfg",
                  "AERON_DIR = /tmp/aeron\n"
                  "VENUE_ID = SIMNOW\n"
                  "ORS_ID = ors\n"
                  "ENVIRONMENT = SHADOW\n"
                  "EXCHANGE = SHFE\n"
                  "INSTRUMENTS = au2612\n"
                  "STREAM_BASE = 9000\n"
                  "SOURCE_PUBLICATION_EPOCH = 1\n");
  const ContractRuntimeConfig defaults = ContractRuntimeConfig::load(optional);
  check(defaults.sourceSessionId == 0, "SOURCE_SESSION_ID defaults to 0");
  check(defaults.queryIntervalSeconds == 1, "QUERY_INTERVAL_SECONDS defaults to 1");

  check(throwsContaining([&] { ContractRuntimeConfig::load(dir / "absent.cfg"); },
                         "cannot open"),
        "missing file is reported");

  if (failures != 0) {
    std::cerr << failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "ctp_runtime_config: strict keys, stream layout and defaults\n";
  return 0;
}
