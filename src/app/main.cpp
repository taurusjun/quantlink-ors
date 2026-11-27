#include <atomic>
#include <exception>
#include <iostream>
#include <string>
#include <thread>

#include "marketdata/MDHandler.h"
#include "session/ORSServer.h"
#include "infra/LogFacade.h"
#include "ors/base/Log.h"
#include "marketupdateNew.h"
#include "transport/AeronClient.h"
#include "transport/AeronOrsTransport.h"

int main(int argc, char** argv) {
  if (argc != 2 && !(argc == 3 && std::string(argv[1]) == "--config")) {
    std::cerr << "usage: ors_ctp --config PATH\n";
    return 2;
  }
  const std::string configPath = argc == 2 ? argv[1] : argv[2];

  try {
    const ::ors::OrsConfig config = ::ors::OrsConfig::load(configPath);
    ::ors::log::init(::ors::base::createLogger("ors", config.logPath));
    ::ors::AeronClient aeron(config);
    ::ors::AeronOrsTransport transport(config, aeron, aeron);
    MDHandler mdHandler(transport);
    MarketUpdateNew marketUpdate{};
    ORSServer server(config, transport, mdHandler, marketUpdate);

    transport.start(
        [&server](::ors::infra::RequestMsg* request) {
          server.ProcessRequest(request);
        },
        [&server](const ::ors::AccountQueryRequest& request) {
          server.QueryAccount(request);
        },
        [&server](const ::ors::ClientRegisterRequest& request) {
          server.RegisterClient(request);
        });

    std::atomic<bool> running{true};
    std::thread pollThread([&] {
      while (running.load(std::memory_order_relaxed)) {
        transport.poll(10);
      }
    });

    server.Login();
    server.Start();
    running.store(false, std::memory_order_relaxed);
    if (pollThread.joinable()) {
      pollThread.join();
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "ORS failed: " << error.what() << '\n';
    return 1;
  }
}
