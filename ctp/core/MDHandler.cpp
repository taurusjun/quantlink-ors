#include <errno.h>
#include <string.h>
#include <iostream>
#include <assert.h>
#include "time.h"

#include "MDHandler.h"
#include "log/Log.h"
#include "log/Metrics.h"

using namespace std;
using namespace illuminati;

int OB_CHECK = 0;
int OB_SHMWRITE = 0;
uint64_t getcurtime()
{
    struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);
    return (tp.tv_sec * 1000000000 + tp.tv_nsec);
}

MDHandler::~MDHandler() = default;

MDHandler::MDHandler(::ors::MarketDataSink& sink)
    : m_seqNum(0), sink_(sink) {
    OB_CHECK = ::ors::metrics().registerStat("OB_CHECK", 2000, 5000);
    OB_SHMWRITE = ::ors::metrics().registerStat("OB_SHMWRITE", 2000, 5000);
}

void MDHandler::ProcessMDUpdate(char *msg)
{
    int64_t startTime, endTime;

    time_t tt;
    time(&tt);

    static time_t prevSec = tt;
    static uint64_t count = 0;

    if (prevSec != tt)
    {
        // std::cout << prevSec << "|"  << count << std::endl;
        count = 1;
        prevSec = tt;
    }
    else
    {
        count++;
    }

    MarketUpdateNew *update = (MarketUpdateNew *)msg;
    // cout << "MDUP|" << up->m_seqnum << "|" << up->m_symbol << endl ;
    /*
     int64_t prevTTQ = TTQMap[std::string(up->m_symbol)];
     if(prevTTQ && prevTTQ > TTQMap[std::string(up->m_symbol)])
     {
     std::cout << up->m_symbol << "|" << up->m_seqnum << "|" << up->m_exchTS << "|" << up->m_lastTradedPrice << "|" << up->m_totalTradedQuantity << "|" << prevTTQ << std::endl;
     std::cout << "Exiting..." << std::endl;
     exit(1);
     }
     TTQMap[std::string(up->m_symbol)] = up->m_totalTradedQuantity;
     */
    update->m_timestamp = getcurtime();
    startTime = update->m_timestamp;
    update->m_seqnum = ++m_seqNum;
    sink_.send(*update);
    endTime = getcurtime();
    ::ors::metrics().record(OB_SHMWRITE, (endTime - startTime));
}
