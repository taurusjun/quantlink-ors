#ifndef _INFRA_RMSCOMMON_H
#define _INFRA_RMSCOMMON_H

#include <map>
#include <unordered_map>
#include "netpositionaggregate_new.h"

namespace illuminati
{
    namespace infra
    {
        typedef std::unordered_map<std::string, illuminati::infra::TickerPositions *> RMSInstrumentCache;
    }
}

#endif
