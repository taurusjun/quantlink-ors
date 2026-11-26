#include "core/ORSServer.h"
#include "transport/InMemoryOrsTransport.h"

int main() {
  static_assert(sizeof(::ORSServer) > 0);
  ::ors::InMemoryOrsTransport transport;
  return transport.responseCount() == 0 ? 0 : 1;
}
