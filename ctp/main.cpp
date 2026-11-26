#include <atomic>
#include <exception>
#include <iostream>
#include <thread>

#include "core/ORSServer.h"
#include "transport/AeronClient.h"
#include "transport/AeronOrsTransport.h"

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: ors_ctp --config PATH\n";
    return 2;
  }

  try {
    const ::ors::OrsConfig config = ::ors::OrsConfig::load(argv[1]);
    ::ors::AeronClient aeron(config);
    ::ors::AeronOrsTransport transport(config, aeron, aeron);
    ORSServer server(config, transport);

    transport.start(
        [&server](illuminati::infra::RequestMsg* request) {
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
