#include <iostream>
#include <string>

#include "ors/ctp/CtpConfig.h"
#include "ors/ctp/ContractRuntime.h"

namespace {

void usage() {
  std::cout << "usage: ors_ctp --config PATH\n"
               "\n"
               "  --config PATH   CTP connection and ORS runtime configuration\n"
               "  --help          print this usage and exit\n"
               "\n"
               "This binary runs the CTP ORS runtime only.\n"
               "Smoke tools are in the separate ors_ctp_smoke binary.\n";
}

}  // namespace

int main(int argc, char** argv) {
  std::string configPath;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--config" && index + 1 < argc) {
      configPath = argv[++index];
    } else if (argument == "--help" || argument == "-h") {
      usage();
      return 0;
    } else {
      std::cerr << "unknown or incomplete option: " << argument << '\n';
      usage();
      return 2;
    }
  }

  if (configPath.empty()) {
    usage();
    return 0;
  }

  try {
    const ors::ctp::CtpConfig ctpConfig = ors::ctp::CtpConfig::load(configPath);
    std::cout << "config: " << ctpConfig.summary() << '\n';

    ors::ctp::ContractRuntimeConfig runtimeConfig =
        ors::ctp::ContractRuntimeConfig::load(configPath);
    ors::ctp::ContractRuntime runtime(ctpConfig, std::move(runtimeConfig));
    return runtime.run();
  } catch (const std::exception& error) {
    std::cerr << "FAILED: " << error.what() << '\n';
    return 1;
  }
}
