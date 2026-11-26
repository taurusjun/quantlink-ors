#ifndef _INFRA_STATS_RECORDER_H_
#define _INFRA_STATS_RECORDER_H_

#include <stdint.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <sstream>
#include "caslock.h"
#include <iomanip>
#include "processsettings.h"
#include "itimer.h"
#include "numtostring.h"

#define STATS_LIMIT 100

#define INIT_STATS(name) \
    int s_idx_##name;    \
    int64_t __startT_##name;
#define DEFINE_STATS(name, min, max) s_idx_##name = illuminati::infra::StatsRecorder::registerNewStat(#name, min, max);
#define RECORD_STATS(name, data) illuminati::infra::StatsRecorder::record(s_idx_##name, data);
#define RECORD_STATS_BEGIN(name) __startT_##name = illuminati::ITime_ClockRT::GetCurrentTime();
#define RECORD_STATS_END(name) illuminati::infra::StatsRecorder::record(s_idx_##name, \
                                                                        illuminati::ITime_ClockRT::GetCurrentTime() - __startT_##name);
#define RECORD_STATS_END_ID(id, name) illuminati::infra::StatsRecorder::record(s_idx_##name, \
                                                                               illuminati::ITime_ClockRT::GetCurrentTime() - __startT_##id);
#define RECORD_STATS_TIME(name, t) illuminati::infra::StatsRecorder::record(s_idx_##name, \
                                                                            illuminati::ITime_ClockRT::GetCurrentTime() - t);

#ifdef ENABLE_INFO_STATS
#define INIT_STATS_INFO(name) INIT_STATS(name)
#define DEFINE_STATS_INFO(name, min, max) DEFINE_STATS(name, min, max)
#define RECORD_STATS_INFO(name, data) RECORD_STATS(name, data)
#define RECORD_STATS_BEGIN_INFO(name) RECORD_STATS_BEGIN(name)
#define RECORD_STATS_END_INFO(name) RECORD_STATS_END(name)
#define RECORD_STATS_END_ID_INFO(id, name) RECORD_STATS_END_ID(id, name)
#define RECORD_STATS_TIME_INFO(name, t) RECORD_STATS_TIME(t)
#else
#define INIT_STATS_INFO(name)
#define DEFINE_STATS_INFO(name, min, max)
#define RECORD_STATS_INFO(name, data)
#define RECORD_STATS_BEGIN_INFO(name)
#define RECORD_STATS_END_INFO(name)
#define RECORD_STATS_END_ID_INFO(id, name)
#define RECORD_STATS_TIME_INFO(name, t)
#endif

#define stat_force_inline inline __attribute__((always_inline))

namespace illuminati
{
    namespace infra
    {
        /*!
         * statData
         */
        struct statData
        {
            uint64_t m_count;
            uint64_t m_total;
            uint64_t m_min;
            uint64_t m_max;
            uint64_t m_upperLimit;
            uint64_t m_lowerLimit;
            uint64_t m_countAboveUpperLimit;
            uint64_t m_countBelowLowerLimit;
        };

        /*!
         * stats
         */
        struct stats
        {
            stats(std::string &name, uint64_t lowerLimit,
                  uint64_t upperLimit)
            {
                m_statsName = name;
                m_data.m_lowerLimit = lowerLimit;
                m_data.m_upperLimit = upperLimit;
                reset();
            }

            ~stats()
            {
            }

            void reset()
            {
                m_data.m_count = 0;
                m_data.m_total = 0;
                m_data.m_min = 0;
                m_data.m_max = 0;
                m_data.m_countAboveUpperLimit = 0;
                m_data.m_countBelowLowerLimit = 0;
            }

            stat_force_inline void update(uint64_t data)
            {
                m_lock.lock();

                m_data.m_count++;
                m_data.m_total += data;
                m_data.m_min =
                    (m_data.m_min == 0) ? data : ((m_data.m_min < data) ? m_data.m_min : data);
                m_data.m_max =
                    (m_data.m_max == 0) ? data : ((m_data.m_max > data) ? m_data.m_max : data);

                if (data >= m_data.m_upperLimit)
                {
                    m_data.m_countAboveUpperLimit++;
                }
                else if (data <= m_data.m_lowerLimit)
                {
                    m_data.m_countBelowLowerLimit++;
                }

                m_lock.unlock();
            }

            statData getStatDataAndReset()
            {
                statData data;
                m_lock.lock();
                data = m_data;
                reset();
                m_lock.unlock();
                return data;
            }
            std::string &getStatName()
            {
                return m_statsName;
            }

        private:
            illuminati::ds::CasLock m_lock;
            statData m_data;
            std::string m_statsName;
        };

        /*!
         * StatsRecorder
         */
        class StatsRecorder
        {
        public:
            /*!
             * getInstance
             */
            stat_force_inline static StatsRecorder &getInstance()
            {
                static StatsRecorder s_statsRec;
                return s_statsRec;
            }

            /*!
             * init
             */
            void init(std::string fileName, int sleepTime = 60)
            {
                if (!m_bInit)
                {
                    m_sleepTime = sleepTime;
                    m_bStopPrinting = false;
                    m_statsCount = 0;

                    m_fileStream.open(fileName, std::ios::app);
                    if (!m_fileStream.good())
                    {
                        std::cout << "Cannot opent the StatsRecorder file"
                                  << fileName << "\n";
                        throw("exception");
                    }

                    std::stringstream ss;
                    ss << "Time  " << std::setw(15) << "Latency"
                       << std::setw(15) << "PacketCount"
                       << std::setw(15) << "Average" << std::setw(15)
                       << "Min" << std::setw(15) << "Max"
                       << std::setw(22) << "Upper Threshold%"
                       << std::setw(25) << "Lower Threshold%\n";
                    m_fileStream.write(ss.str().c_str(),
                                       ss.str().length());
                    m_bInit = true;
                    m_printThread = std::thread(&StatsRecorder::printStats,
                                                this);

#ifdef STATS_DUMP
                    // Initialize Logger for dumping all the samples.
                    Logger::InitLogger(LOGGERTYPE::STRATEGY, LOGGEROUT::TO_FILE, fileName + ".dump");
#endif
                }
            }

            /*!
             * registerNewStat
             */
            static int registerNewStat(std::string name,
                                       uint64_t lowerLimit, uint64_t upperLimit)
            {
                return (getInstance().registerStat(name, lowerLimit,
                                                   upperLimit));
            }

            /*!
             * record
             */
            stat_force_inline static void record(int token, uint64_t data)
            {
                getInstance().recordStat(token, data);
                return;
            }

            /*!
             * dtor
             */
            ~StatsRecorder()
            {
                if (m_bfinished)
                    return;
                m_bStopPrinting = true;
                m_bInit = false;

                if (m_printThread.joinable())
                {
                    m_printThread.join();
                }

                for (int i = 0; i < m_statsCount; i++)
                {
                    delete m_statsList[i];
                }
            }
            void finish()
            {
                m_bStopPrinting = true;
                m_bInit = false;

                if (m_printThread.joinable())
                {
                    m_printThread.join();
                }

                for (int i = 0; i < m_statsCount; i++)
                {
                    delete m_statsList[i];
                }
                m_bfinished = true;
            }

        private:
            int registerStat(std::string &name, uint64_t lowerLimit,
                             uint64_t upperLimit)
            {
                if (!m_bInit)
                {
                    return -1;
                }

                if (m_statsCount >= STATS_LIMIT - 1)
                {
                    std::cout
                        << "StatsRecorder: no of registration exceeds the MAX LIMIT:"
                        << m_statsCount << "\n";
                    m_bInit = false;
                    return -1;
                }

                int32_t token = m_statsCount;
                m_statsList[token] = new stats(name, lowerLimit,
                                               upperLimit);
                std::cout << "StatsRecorder: registering new stat name:"
                          << name << " registrationId:" << token << "\n";
                m_statsCount++;
                return token;
            }

            stat_force_inline void recordStat(int token, uint64_t data)
            {
                if (!m_bInit || token == -1)
                {
                    return;
                }

                if (token >= m_statsCount)
                {
                    // Print this into logs once, we don't want to log for ever or assert
                    static bool printOverflow = true;
                    if (printOverflow)
                        std::cout << "StatsRecorder:Invalid Stat token id "
                                  << token << "\n";
                    printOverflow = false;
                    return;
                }
#ifdef STATS_DUMP
                char statBuffer[1024];
                sprintf(statBuffer, "%s:%llu", m_statsList[token]->getStatName(), data);
                LOGQ(MSG, statBuffer);
#endif
                m_statsList[token]->update(data);
            }

            void printStats()
            {

                illuminati::sysinfo::ProcessSettings ps;
                ps.configure("STATS_COLLECTOR_THREAD");

                time_t counter;
                struct tm timeinfo;
                stats *cur = NULL;

                while (true)
                {
                    std::time(&counter);
                    ::localtime_r(&counter, &timeinfo);

                    std::stringstream ss;
                    bool flush = false;
                    for (int32_t i = 0; i < m_statsCount; i++)
                    {
                        cur = m_statsList[i];
                        statData stat = cur->getStatDataAndReset();

                        if (stat.m_count)
                        {
                            flush = true;
                            ss << timeinfo.tm_hour << ":" << timeinfo.tm_min
                               << " " << std::setw(15)
                               << cur->getStatName().c_str()
                               << std::setw(15) << stat.m_count
                               << std::setw(15)
                               << (stat.m_total / (stat.m_count))
                               << std::setw(15) << stat.m_min
                               << std::setw(15) << stat.m_max
                               << std::setw(8) << "[>"
                               << std::setw(10) << stat.m_upperLimit
                               << "]" << std::setw(3)
                               << ((stat.m_countAboveUpperLimit * 100) / stat.m_count) << std::setw(8)
                               << "[<" << std::setw(10)
                               << stat.m_lowerLimit << "]"
                               << std::setw(3)
                               << ((stat.m_countBelowLowerLimit * 100) / stat.m_count) << "\n";
                        }
                    }

                    if (flush)
                    {
                        ss << "\n";
                        m_fileStream.write(ss.str().c_str(),
                                           ss.str().length());
                        m_fileStream.flush();
                    }
                    else if (m_bStopPrinting)
                        break;
                    sleep(m_sleepTime);
                }
            }

        private:
            StatsRecorder() : m_bInit(false),
                              m_bStopPrinting(true),
                              m_statsCount(0),
                              m_bfinished(false),
                              m_sleepTime(60) // 60 seconds
                              {};
            StatsRecorder(const StatsRecorder &);
            StatsRecorder &operator=(const StatsRecorder &);

            bool m_bInit;
            bool m_bStopPrinting;
            int m_statsCount;
            stats *m_statsList[STATS_LIMIT];
            std::thread m_printThread;
            std::ofstream m_fileStream;
            bool m_bfinished;
            int m_sleepTime;
        };
    }
}
#endif
