#include <exception>
#include <iostream>
#include <string>

#include "ors/ctp/CtpConfig.h"
#include "ors/ctp/CtpSession.h"

namespace {

void usage() {
  std::cout << "usage: ors_ctp [--config PATH] [--mode login-smoke]\n"
               "\n"
               "  (no arguments)          print this usage and exit\n"
               "  --mode login-smoke      log in to CTP, confirm settlement, query\n"
               "                          instruments, print READY, exit 0\n";
}

}  // namespace

int main(int argc, char** argv) {
  std::string configPath;
  std::string mode;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--config" && index + 1 < argc) {
      configPath = argv[++index];
    } else if (argument == "--mode" && index + 1 < argc) {
      mode = argv[++index];
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
  if (mode != "login-smoke") {
    std::cerr << "unsupported mode: " << mode << '\n';
    usage();
    return 2;
  }
  if (configPath.empty()) {
    std::cerr << "--config is required for --mode login-smoke\n";
    return 2;
  }

  try {
    const ors::ctp::CtpConfig config = ors::ctp::CtpConfig::load(configPath);
    std::cout << "config: " << config.summary() << '\n';

    ors::ctp::CtpTraderSession session(config);
    if (!session.start()) {
      std::cerr << "FAILED: " << session.lastError() << '\n';
      return 1;
    }
    std::cout << toString(session.state()) << " instruments="
              << session.instruments().size() << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAILED: " << error.what() << '\n';
    return 1;
  }
}
