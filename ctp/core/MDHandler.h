#ifndef _MD_HANDLER_H
#define _MD_HANDLER_H

#include <string.h>
#include <unordered_map>
#include <map>
#include <list>

#include "marketupdateNew.h"
#include "md/MarketDataSink.h"

#include "MdSpi.h"

#define MD_SHMKEY 871 // lzd change 871 to 872
#define MD_SHMSIZE 100000

using namespace ors::md;

class MDHandler
{
public:
    explicit MDHandler(::ors::MarketDataSink& sink);
    ~MDHandler();
    void ProcessMDUpdate(char *);

private:
    uint64_t m_seqNum;
    uint64_t m_exchTS;
    ::ors::MarketDataSink& sink_;
};

#endif
