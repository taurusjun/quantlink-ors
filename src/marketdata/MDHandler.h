#pragma once

#include <cstdint>

#include "marketupdateNew.h"
#include "marketdata/MarketDataSink.h"

class MDHandler
{
public:
    explicit MDHandler(::ors::MarketDataSink& sink);
    ~MDHandler();
    void ProcessMDUpdate(char *);

private:
    std::uint64_t m_seqNum{0};
    std::uint64_t m_exchTS{0};
    ::ors::MarketDataSink& sink_;
};
