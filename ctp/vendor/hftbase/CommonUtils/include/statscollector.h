/*
 * statscollector.h
 *
 *  Created on: 04-Feb-2014
 *      Author: jijith
 */

#ifndef _INFRA_STATS_COLLECTOR_H_
#define _INFRA_STATS_COLLECTOR_H_

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
#include "circularqueue.hpp"
#include "queuereader.hpp"
#include "queuesinglewriter.hpp"
#include "ArrayBasedStoragePolicy.h"
#include <cstdint>
#include <ctime>

namespace illuminati
{
    namespace stats
    {
        static const int MAX_STATS = 100;

        struct BaseTokenInfo
        {
            uint64_t latency;
            int token;
        };

        struct TokenInfo : public BaseTokenInfo
        {
            int uniqueId;

            static void rawHeader(std::stringstream &ss)
            {
                ss << "Token|Type|Latency";
            }

            void toIntervalString(std::stringstream &ss)
            {
                ss << latency;
            }

            void toAbsoluteString(std::stringstream &ss)
            {
                ss << latency << "|";
                ss << uniqueId;
            }
        };

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
            stats(std::string &name)
            {
                m_statsName = name;
                m_isIncludedInTimely = false;
                reset();
            }
            stats(std::string &name, uint64_t lowerLimit,
                  uint64_t upperLimit)
            {
                m_statsName = name;
                m_data.m_lowerLimit = lowerLimit;
                m_data.m_upperLimit = upperLimit;
                m_isIncludedInTimely = true;
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

            void update(uint64_t data)
            {
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
            }

            statData getStatDataAndReset()
            {
                statData data;
                data = m_data;
                reset();
                return data;
            }
            std::string &getStatName()
            {
                return m_statsName;
            }

            bool isIncludedInTimely()
            {
                return m_isIncludedInTimely;
            }

        private:
            statData m_data;
            std::string m_statsName;
            bool m_isIncludedInTimely;
        };
        /*!
         * StatsCollector
         */
        template <typename T = TokenInfo>
        class StatsCollector
        {
        public:
            /*!
             * getInstance
             */
            static StatsCollector &getInstance()
            {
                static StatsCollector s_statsRec;
                return s_statsRec;
            }

            /*!
             * init
             */
            void init(std::string fileName, int sampleCollectionTime = 60,
                      int sleepTime = 20)
            {
                if (!m_bInit)
                {
                    m_sleepTime = sleepTime;
                    m_sampleCollectionTime = sampleCollectionTime;
                    m_bStopPrinting = false;
                    m_statsCount = 0;

                    std::string timelyFile = fileName + ".timely";
                    m_timelyFile.open(timelyFile, std::ios::app);
                    if (!m_timelyFile.good())
                    {
                        std::cout
                            << "Cannot open the timely StatsRecorder file"
                            << fileName << "\n";
                        throw("Unable to open timely file");
                    }
                    m_rawFile.open(fileName, std::ios::app);
                    if (!m_rawFile.good())
                    {
                        std::cout
                            << "Cannot open the raw StatsRecorder file"
                            << fileName << "\n";
                        throw("Unable to open raw file");
                    }

                    m_ringReader = m_ringBuffer.createReader();
                    m_ringWriter = m_ringBuffer.createSingleWriter();

                    std::stringstream ss;
                    ss << "Time  " << std::setw(15) << "Latency"
                       << std::setw(15) << "PacketCount"
                       << std::setw(15) << "Average" << std::setw(15)
                       << "Min" << std::setw(15) << "Max"
                       << std::setw(22) << "Upper Threshold%"
                       << std::setw(25) << "Lower Threshold%\n";
                    m_timelyFile.write(ss.str().c_str(),
                                       ss.str().length());

                    ss.str("");

                    T::rawHeader(ss);
                    ss << "\n";
                    m_rawFile.write(ss.str().c_str(),
                                    ss.str().length());

                    m_bInit = true;
                    m_printThread = std::thread(
                        &StatsCollector::printStats, this);
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

            static int registerNewStat(std::string name)
            {
                return (getInstance().registerStat(name));
            }

            /*!
             * record
             */
            static void record(int token, uint64_t data)
            {
                getInstance().recordStat(token, data);
                return;
            }

            static void record(const T &tokenInfo)
            {
                getInstance().recordStat(tokenInfo);
                return;
            }

            /*!
             * dtor
             */
            ~StatsCollector()
            {
                if (m_bfinished)
                    return;
                finish();
            }
            void finish()
            {
                m_bStopPrinting = true;
                m_bInit = false;

                if (m_printThread.joinable())
                {
                    m_printThread.join();
                }
                m_rawFile.close();

                for (int i = 0;
                     i < m_statsCount.load(std::memory_order_relaxed);
                     i++)
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
                // check if there is already a stats with the same name created.
                // If there is, then return the corresponding index
                for (int i = 0;
                     i < m_statsCount.load(std::memory_order_relaxed);
                     i++)
                {
                    if (m_statsList[i]->getStatName() == name)
                    {
                        ILOG(INFO) << "Stats: " << name << " already registered\n";
                        return i;
                    }
                }

                if (m_statsCount.load(std::memory_order_acquire) >= MAX_STATS - 1)
                {
                    std::cout
                        << "StatsRecorder: no of registration exceeds the MAX LIMIT:"
                        << m_statsCount << "\n";
                    m_bInit = false;
                    return -1;
                }

                int32_t token = m_statsCount.fetch_add(1,
                                                       std::memory_order_acq_rel);

                m_statsList[token] = new stats(name, lowerLimit,
                                               upperLimit);
                std::cout << "StatsRecorder: registering new stat name:"
                          << name << " registrationId:" << token << "\n";
                return token;
            }

            int registerStat(std::string &name)
            {
                if (!m_bInit)
                {
                    return -1;
                }

                if (m_statsCount.load(std::memory_order_acquire) >= MAX_STATS - 1)
                {
                    std::cout
                        << "StatsRecorder: no of registration exceeds the MAX LIMIT:"
                        << m_statsCount << "\n";
                    m_bInit = false;
                    return -1;
                }

                int32_t token = m_statsCount.fetch_add(1,
                                                       std::memory_order_acq_rel);
                m_statsList[token] = new stats(name);
                std::cout << "StatsRecorder: registering new stat name:"
                          << name << " registrationId:" << token << "\n";
                return token;
            }

            void recordStat(int token, uint64_t data)
            {
                if (!m_bInit || token == -1)
                {
                    return;
                }

                // To improve the performance we are using relaxed mode.
                // Its safe to use relaxed mode here as its only a bounds check and could potentially be removed.
                if (token >= m_statsCount.load(std::memory_order_relaxed))
                {
                    // Print this into logs once, we don't want to log for ever or assert
                    static bool printOverflow = true;
                    if (printOverflow)
                        std::cout << "StatsRecorder:Invalid Stat token id "
                                  << token << "\n";
                    printOverflow = false;
                    return;
                }
                // m_statsList[token]->update (data);

                T tokenInfo;
                tokenInfo.token = token;
                tokenInfo.latency = data;
                // std::cout<<data<<"\n";
                m_ringWriter->enqueue(tokenInfo);
            }

            void recordStat(const T &tokenInfo)
            {
                if (!m_bInit || tokenInfo.token == -1)
                {
                    return;
                }

                if (tokenInfo.token >= m_statsCount.load(std::memory_order_relaxed))
                {
                    // Print this into logs once, we don't want to log for ever or assert
                    static bool printOverflow = true;
                    if (printOverflow)
                        std::cout << "StatsRecorder:Invalid Stat token id "
                                  << tokenInfo.token << "\n";
                    printOverflow = false;
                    return;
                }
                m_ringWriter->enqueue(tokenInfo);
            }

            void handleTimelyFlush(tm &timeinfo)
            {
                stats *cur = NULL;
                bool flush = false;

                std::stringstream ss;
                for (int32_t i = 0;
                     i < m_statsCount.load(std::memory_order_acquire);
                     i++)
                {
                    cur = m_statsList[i];
                    statData stat = cur->getStatDataAndReset();

                    if (stat.m_count && cur->isIncludedInTimely())
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
                           << std::setw(8) << "[>" << std::setw(10)
                           << stat.m_upperLimit << "]" << std::setw(3)
                           << ((stat.m_countAboveUpperLimit * 100) / stat.m_count) << std::setw(8)
                           << "[<" << std::setw(10)
                           << stat.m_lowerLimit << "]" << std::setw(3)
                           << ((stat.m_countBelowLowerLimit * 100) / stat.m_count) << "\n";
                    }
                }

                if (flush)
                {
                    ss << "\n";
                    m_timelyFile.write(ss.str().c_str(),
                                       ss.str().length());
                    m_timelyFile.flush();
                }
            }

            void printStats()
            {

                illuminati::sysinfo::ProcessSettings ps;
                ps.configure("STATS_COLLECTOR_THREAD");

                time_t counter;
                time_t prevTime = std::time(NULL);
                struct tm timeinfo;
                stats *cur = NULL;

                std::stringstream ss;
                while (!m_bStopPrinting)
                {
                    long qsize = m_ringReader->getSize();
                    for (long i = 0; i < qsize; i++)
                    {
                        if (m_ringReader->isEmpty())
                            break;

                        T ti = m_ringReader->dequeue();
                        // std::cout<<ti.token<<"|"<<ti.latency<<"\n";

                        if (ti.token >= m_statsCount.load(
                                            std::memory_order_acquire))
                        {
                            continue;
                        }

                        cur = m_statsList[ti.token];
                        ss << cur->getStatName().c_str() << "|";
                        if (cur->isIncludedInTimely())
                        {
                            cur->update(ti.latency);
                            ss << "Interval|";
                            ti.toIntervalString(ss);
                            ss << "\n";
                        }
                        else
                        {
                            ss << "Absolute|";
                            ti.toAbsoluteString(ss);
                            ss << "\n";
                        }
                    }

                    sleep(m_sleepTime);
                    std::time(&counter);
                    if (counter - prevTime > m_sampleCollectionTime)
                    {
                        if (ss.str().length() > 0)
                        {
                            m_rawFile.write(ss.str().c_str(),
                                            ss.str().length());
                            m_rawFile.flush();
                        }
                        ss.str(std::string(""));

                        ::localtime_r(&counter, &timeinfo);
                        handleTimelyFlush(timeinfo);
                        prevTime = counter;
                    }

                    /*						while(!m_ringReader->isEmpty())
                     {
                     std::time (&counter);
                     T ti = m_ringReader->dequeue();

                     //std::cout<<ti.token<<"|"<<ti.latency<<"\n";

                     cur = m_statsList[ti.token];
                     if(cur->isIncludedInTimely())
                     cur->update(ti.latency);

                     ss << cur->getStatName().c_str() << "|";
                     if(cur->isIncludedInTimely())
                     {
                     ss << "Interval|";
                     ti.toIntervalString(ss);
                     ss << "\n";
                     }
                     else
                     {
                     ss << "Absolute|";
                     ti.toAbsoluteString(ss);
                     ss << "\n";

                     }

                     if(counter - prevTime > m_sampleCollectionTime)
                     {
                     if(ss.str().length() > 0)
                     {
                     m_rawFile.write(ss.str().c_str(), ss.str().length());
                     m_rawFile.flush();
                     }
                     ss.str(std::string(""));

                     ::localtime_r (&counter, &timeinfo);
                     handleTimelyFlush(timeinfo);
                     prevTime = counter;
                     }
                     }*/
                }

                if (ss.str().length() > 0)
                {
                    m_rawFile.write(ss.str().c_str(),
                                    ss.str().length());
                    m_rawFile.flush();
                    ss.str(std::string(""));
                }

                ::localtime_r(&counter, &timeinfo);
                handleTimelyFlush(timeinfo);
            }

        private:
            StatsCollector() : m_bInit(false), m_bStopPrinting(true), m_statsCount(
                                                                          0),
                               m_bfinished(false), m_sleepTime(20), // 20 seconds
                               m_sampleCollectionTime(60), m_ringReader(0), m_ringWriter(
                                                                                0)
            {
            }

            StatsCollector(const StatsCollector &) = delete;
            StatsCollector &operator=(const StatsCollector &) = delete;

            bool m_bInit;
            bool m_bStopPrinting;
            std::atomic_int m_statsCount;
            stats *m_statsList[MAX_STATS];
            std::thread m_printThread;
            std::ofstream m_timelyFile;
            std::ofstream m_rawFile;
            bool m_bfinished;
            int m_sleepTime;
            int m_sampleCollectionTime;

            typedef CircularQR(T, illuminati::storagepolicy::ArrayStorage, INT(1000000)) RingReader;
            typedef CircularQSW(T, illuminati::storagepolicy::ArrayStorage, INT(1000000)) RingWriter;
            typedef CircularQMultiAccess(T, illuminati::storagepolicy::ArrayStorage, INT(1000000)) RingBuffer;

            RingBuffer m_ringBuffer;
            RingReader *m_ringReader;
            RingWriter *m_ringWriter;
        };
    }
}
#endif
