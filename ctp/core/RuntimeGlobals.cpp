#include <cstdint>

#include "MDHandler.h"
#include "ORSServer.h"
#include "marketupdateNew.h"

// The imported company MD path still uses these process globals. Task 10 will
// replace them with an explicit runtime object.
int32_t g_RequestId = 0;
struct MarketUpdateNew *g_pUpdate = nullptr;
MDHandler *g_pMDHandler = nullptr;
ORSServer *g_pORS = nullptr;
