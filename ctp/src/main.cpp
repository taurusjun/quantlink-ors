#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "ors/ctp/CtpConfig.h"
#include "ors/ctp/ContractRuntime.h"

namespace {

void usage() {
  std::cout << "usage: ors_ctp --config PATH --mode runtime\n"
               "              --aeron-dir DIR --instruments ID[,ID...]\n"
               "              [--venue-id ID] [--ors-id ID] [--environment NAME]\n"
               "              [--exchange NAME] [--source-session-id N]\n"
               "              [--source-publication-epoch N] [--stream-base N]\n"
               "\n"
               "  (no arguments)          print this usage and exit\n"
               "  --mode runtime          run the CTP ORS runtime\n"
               "\n"
               "Smoke tools are in the separate ors_ctp_smoke binary.\n";
}

std::vector<std::string> splitList(const std::string& value) {
  std::vector<std::string> items;
  std::string current;
  for (const char character : value) {
    if (character == ',' || character == ' ') {
      if (!current.empty()) {
        items.push_back(current);
        current.clear();
      }
    } else {
      current.push_back(character);
    }
  }
  if (!current.empty()) {
    items.push_back(current);
  }
  return items;
}

}  // namespace

int main(int argc, char** argv) {
  std::string configPath;
  std::string mode;
  std::string instrumentList;
  std::string aeronDir;
  std::string venueId = "SIMNOW";
  std::string orsId = "ors-ctp-1";
  std::string environment = "SHADOW";
  std::string exchange = "SHFE";
  unsigned long long sourceSessionId = 0;
  unsigned long long sourcePublicationEpoch = 1;
  int streamBase = 9000;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--config" && index + 1 < argc) {
      configPath = argv[++index];
    } else if (argument == "--mode" && index + 1 < argc) {
      mode = argv[++index];
    } else if (argument == "--instruments" && index + 1 < argc) {
      instrumentList = argv[++index];
    } else if (argument == "--aeron-dir" && index + 1 < argc) {
      aeronDir = argv[++index];
    } else if (argument == "--venue-id" && index + 1 < argc) {
      venueId = argv[++index];
    } else if (argument == "--ors-id" && index + 1 < argc) {
      orsId = argv[++index];
    } else if (argument == "--environment" && index + 1 < argc) {
      environment = argv[++index];
    } else if (argument == "--exchange" && index + 1 < argc) {
      exchange = argv[++index];
    } else if (argument == "--source-session-id" && index + 1 < argc) {
      sourceSessionId = std::stoull(argv[++index]);
    } else if (argument == "--source-publication-epoch" && index + 1 < argc) {
      sourcePublicationEpoch = std::stoull(argv[++index]);
    } else if (argument == "--stream-base" && index + 1 < argc) {
      streamBase = std::stoi(argv[++index]);
    } else if (argument == "--help" || argument == "-h") {
      usage();
      return 0;
    } else {
      std::cerr << "unknown or incomplete argument: " << argument << '\n';
      usage();
      return 2;
    }
  }

  if (mode.empty()) {
    if (!configPath.empty()) {
      std::cerr << "--mode is required with --config\n";
      return 2;
    }
    usage();
    return 0;
  }
  if (mode != "runtime") {
    std::cerr << "unsupported mode: " << mode
              << " (smoke modes live in ors_ctp_smoke)\n";
    usage();
    return 2;
  }
  if (configPath.empty() || aeronDir.empty() || instrumentList.empty()) {
    std::cerr << "--config, --aeron-dir and --instruments are required for --mode runtime\n";
    return 2;
  }

  try {
    const ors::ctp::CtpConfig config = ors::ctp::CtpConfig::load(configPath);
    std::cout << "config: " << config.summary() << '\n';

    ors::ctp::ContractRuntimeConfig runtimeConfig;
    runtimeConfig.aeronDir = aeronDir;
    runtimeConfig.venueId = venueId;
    runtimeConfig.orsId = orsId;
    runtimeConfig.environment = environment;
    runtimeConfig.exchange = exchange;
    runtimeConfig.sourceSessionId = sourceSessionId;
    runtimeConfig.sourcePublicationEpoch = sourcePublicationEpoch;
    runtimeConfig.instruments = splitList(instrumentList);
    runtimeConfig.orderStream = streamBase + 2;
    runtimeConfig.cancelStream = streamBase + 3;
    runtimeConfig.streams = {streamBase + 4, streamBase + 5, streamBase + 6,
                             streamBase + 7, streamBase + 1};
    ors::ctp::ContractRuntime runtime(config, std::move(runtimeConfig));
    return runtime.run();
  } catch (const std::exception& error) {
    std::cerr << "FAILED: " << error.what() << '\n';
    return 1;
  }
}
