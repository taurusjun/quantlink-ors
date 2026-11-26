/*
 * Company: Illuminati
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-illuminati.com/
 */
/////////////////////////////////////////////////////////////////////////////////
// use -lrt at the end of g++ command
////////////////////////////////////////////////////////////////////////////////

#ifndef _ITIMER_H_
#define _ITIMER_H_

#include <time.h>
#include <inttypes.h>
#include <string>
#include <string.h>
#include <chrono>
#include <stringutils.h>
#include <mktime_internal.h>
#include "gmtime_internal.h"

namespace illuminati
{

    typedef std::chrono::high_resolution_clock hres_clock;
    typedef std::chrono::milliseconds milliseconds;
    typedef std::chrono::microseconds microseconds;
    typedef std::chrono::nanoseconds nanoseconds;
    typedef std::chrono::seconds seconds;
    typedef std::chrono::minutes minutes;

#define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)

#if GCC_VERSION > 40700
    typedef std::chrono::system_clock monotinic_clock;
#else
    typedef std::chrono::monotonic_clock monotinic_clock;
#endif

    /*---------------------------------------------------------------------------
     * ITime_HighResolution
     * high_resolution_clock is the clock with the shortest tick period.
     * this may be a synonym for system_clock or monotinic_clock.
     *----------------------------------------------------------------------------
     */
    class ITime_HighResolution
    {
    private:
        hres_clock::time_point m_start, m_end;

    public:
        void start() { m_start = hres_clock::now(); }
        void stop() { m_end = hres_clock::now(); }
        void pause() { stop(); }

        int64_t getelapsedMinutes() { return (std::chrono::duration_cast<minutes>(m_end - m_start)).count(); }
        int64_t getelapsedSecs() { return (std::chrono::duration_cast<seconds>(m_end - m_start)).count(); }
        int64_t getelapsedMillSecs() { return (std::chrono::duration_cast<milliseconds>(m_end - m_start)).count(); }
        int64_t getelapsedMicroSecs() { return (std::chrono::duration_cast<microseconds>(m_end - m_start)).count(); }
        int64_t getelapsedNanoSecs() { return (std::chrono::duration_cast<nanoseconds>(m_end - m_start)).count(); }
    };

    /*---------------------------------------------------------------------------
     * ITime_Monotinic
     * wrapper for monotonic clock.
     * should be using this for profiling as the clock never goes back
     * need to check if this is using CLOCK_MONOTONIC_RAW (since Linux 2.6.28)
     *---------------------------------------------------------------------------
     */
    class ITime_Monotinic
    {
    private:
        monotinic_clock::time_point m_start, m_end;

    public:
        void start() { m_start = monotinic_clock::now(); }
        void stop() { m_end = monotinic_clock::now(); }
        void pause() { stop(); }

        int64_t getelapsedMinutes() { return (std::chrono::duration_cast<minutes>(m_end - m_start)).count(); }
        int64_t getelapsedSecs() { return (std::chrono::duration_cast<seconds>(m_end - m_start)).count(); }
        int64_t getelapsedMillSecs() { return (std::chrono::duration_cast<milliseconds>(m_end - m_start)).count(); }
        int64_t getelapsedMicroSecs() { return (std::chrono::duration_cast<microseconds>(m_end - m_start)).count(); }
        int64_t getelapsedNanoSecs() { return (std::chrono::duration_cast<nanoseconds>(m_end - m_start)).count(); }
    };

    template <typename T>
    class Timer : public T
    {
    private:
        uint64_t value;

    public:
        Timer() : value(0) {}

        void reset()
        {
            value = 0;
        }

        void stop()
        {
            T::stop();
            value += T::getelapsedNanoSecs();
        }

        uint64_t getTotalTime()
        {
            return value;
        }
    };

    class ITime_StatsClock
    {
    private:
        uint64_t m_start;
        uint64_t m_end;

    public:
        inline __attribute__((always_inline)) void start()
        {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            m_start = ((ts.tv_sec * 1000000000) + ts.tv_nsec);
        }

        inline __attribute__((always_inline)) void stop()
        {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            m_end = ((ts.tv_sec * 1000000000) + ts.tv_nsec);
        }

        uint64_t getelapsedNanoSecs()
        {
            return m_end - m_start;
        }
    };

    /*---------------------------------------------------------------------------
     * ITime_ClockRT
     * wrapper for system-wide real-time clock
     *---------------------------------------------------------------------------
     */
    class ITime_ClockRT
    {
    public:
        inline __attribute__((always_inline)) static uint64_t GetCurrentTime(void)
        {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            return ((ts.tv_sec * 1000000000) + ts.tv_nsec);
        }

        /*!
         *  Make sure that inbuff is allocated to accomodate 21 + 1('\0') chars
         *  Output Format = 20131007-16:23:25.179 ==> 21 chars
         */
        inline __attribute__((always_inline)) static char *GetFIXUTCTimeMilli(char *inCharSz22)
        {
            uint64_t currentTime = GetCurrentTime();
            time_t epoch = (time_t)currentTime / 1000000000LL;
            int millisec = ((currentTime % 1000000000LL) / 1000000LL);

            struct tm t;
            gmtime_r_internal(&epoch, &t);
            size_t len;

            intTocharsWPadding(inCharSz22, 5, t.tm_year + 1900, len, 4, '0');
            intTocharsWPadding(inCharSz22 + 4, 3, t.tm_mon + 1, len, 2, '0');
            intTocharsWPadding(inCharSz22 + 6, 3, t.tm_mday, len, 2, '0');
            inCharSz22[8] = '-';
            intTocharsWPadding(inCharSz22 + 9, 3, t.tm_hour, len, 2, '0');
            inCharSz22[11] = ':';
            intTocharsWPadding(inCharSz22 + 12, 3, t.tm_min, len, 2, '0');
            inCharSz22[14] = ':';
            intTocharsWPadding(inCharSz22 + 15, 3, t.tm_sec, len, 2, '0');
            inCharSz22[17] = '.';
            intTocharsWPadding(inCharSz22 + 18, 4, millisec, len, 3, '0');

            return inCharSz22;
        }

        /*!
         * same as above, for benchmarking - Dont use this method
         */
        inline __attribute__((always_inline)) static char *GetFIXUTCTimeMilliSprint(char *inCharSz22)
        {
            uint64_t currentTime = GetCurrentTime();
            time_t epoch = (time_t)currentTime / 1000000000LL;
            int millisec = ((currentTime % 1000000000LL) / 1000000LL);

            struct tm t;
            gmtime_r(&epoch, &t);

            sprintf(inCharSz22, "%4d%02d%02d-%02d:%02d:%02d.%03d",
                    t.tm_year + 1900,
                    t.tm_mon + 1,
                    t.tm_mday,
                    t.tm_hour,
                    t.tm_min,
                    t.tm_sec,
                    millisec);

            return inCharSz22;
        }

        /*!
         *  Make sure that inbuff is allocated to accomodate 17 + 1('\0') chars
         *  Output Format = 20131007-16:23:25 ==> 17 chars
         */
        inline __attribute__((always_inline)) static char *GetFIXUTCTime(char *inCharSz18)
        {
            static const char *fixFormat = "%4d%02d%02d-%02d:%02d:%02d";
            uint64_t currentTime = GetCurrentTime();
            time_t epoch = (time_t)currentTime / 1000000000LL;

            struct tm t;
            gmtime_r(&epoch, &t);

            sprintf(inCharSz18, fixFormat,
                    t.tm_year + 1900,
                    t.tm_mon + 1,
                    t.tm_mday,
                    t.tm_hour,
                    t.tm_min,
                    t.tm_sec);

            return inCharSz18;
        }

        /*!
         * GetSecondsFromEpochTillMidnight
         */
        static time_t GetSecondsFromEpochTillMidnight()
        {
            uint64_t currentTime = GetCurrentTime();
            time_t epoch = (time_t)currentTime / 1000000000LL;

            struct tm t;
            gmtime_r(&epoch, &t);
            // set the time to 00:00:00 (midnight)
            t.tm_hour = 0;
            t.tm_sec = 0;
            t.tm_min = 0;
            time_t secondsFromEpockTillMidNite = infra::mktime_internal(&t);
            return secondsFromEpockTillMidNite;
        }
    };

    /*---------------------------------------------------------------------------
     * ITime_RDTSC. Use tsc counter directly
     *----------------------------------------------------------------------------
     */

    class ITime_RDTSC
    {
    private:
        static inline uint64_t &m_tscbase()
        {
            static uint64_t _x;
            return _x;
        }
        static inline uint64_t &m_ref()
        {
            static uint64_t _x;
            return _x;
        }
        static inline uint64_t &m_scale()
        {
            static uint64_t _x;
            return _x;
        }
        static inline uint64_t &m_resetlimit()
        {
            static uint64_t _x;
            return _x;
        }
        uint64_t m_start, m_end;
        static const uint64_t MAX_RETRIES = 1000;

        static uint64_t _start()
        {
            uint64_t cycles_high, cycles_low;
            asm volatile("cpuid\n\t"
                         "rdtsc\n\t"
                         "mov %%rdx, %0\n\t"
                         "mov %%rax, %1\n\t"
                         : "=r"(cycles_high), "=r"(cycles_low)::"%rax", "%rbx", "%rcx", "%rdx");

            return (cycles_high << 32) + cycles_low;
        }

        static uint64_t _stop()
        {
            uint64_t cycles_high, cycles_low;
            asm volatile("rdtscp\n\t"
                         "mov %%rdx, %0\n\t"
                         "mov %%rax, %1\n\t"
                         "cpuid\n\t"
                         : "=r"(cycles_high), "=r"(cycles_low)::"%rax", "%rbx", "%rcx", "%rdx");
            return (cycles_high << 32) + cycles_low;
        }
        static const uint64_t _interval = (1ull << 24);

    public:
        static void init()
        {
            uint64_t cycles_high, cycles_low;

            uint64_t rt2;
            uint64_t err = _interval;
            for (unsigned int i = 0; i < MAX_RETRIES; i++)
            {
                uint64_t _tsc1 = _start();
                uint64_t _rt1 = illuminati::ITime_ClockRT::GetCurrentTime();

                uint64_t _tsc2 = _stop();
                while (_tsc2 - _tsc1 < _interval)
                    _tsc2 = _stop();
                uint64_t _rt2 = illuminati::ITime_ClockRT::GetCurrentTime();

                uint64_t _err;
                if ((_tsc2 - _tsc1) > _interval)
                    _err = (_tsc2 - _tsc1) - _interval;
                else
                    _err = _interval - (_tsc2 - _tsc1);

                if (_err < err)
                {
                    err = _err;
                    m_tscbase() = _tsc1;
                    m_ref() = _rt1;
                    rt2 = _rt2;
                }
            }
            std::cout << err << "\n";
            m_scale() = rt2 - m_ref();
            uint64_t max_val = 1ull << 63;
            m_resetlimit() = (max_val / m_scale()) >> 1;
            std::cout << m_resetlimit() << std::endl;
        }

        static inline uint64_t GetCurrentTime()
        {
            uint64_t cycles_high, cycles_low;
            // asm volatile ( "rdtscp\n" : "=a" (rax), "=d" (rdx), "=c" (aux) : : );
            asm volatile("rdtscp\n\t"
                         "mov %%rdx, %0\n\t"
                         "mov %%rax, %1\n\t"
                         //    "cpuid\n\t"
                         : "=r"(cycles_high), "=r"(cycles_low)::"%rax", "%rbx", "%rcx", "%rdx");
            uint64_t tsc = (cycles_high << 32) + cycles_low;

            /*
            if (tsc - m_tscbase > 100000) {
              m_ref = ( (((tsc - m_tscbase) >> 8)  * m_scale) ) + m_ref; //ITime_ClockRT::GetCurrentTime();
              m_tscbase = tsc;
              return m_ref;
            }
            */
            // return ( (((tsc - m_tscbase) >> 32)  * m_scale) ) + m_ref;
            if (tsc - m_tscbase() > m_resetlimit())
            {
                m_ref() = illuminati::ITime_ClockRT::GetCurrentTime();
                m_tscbase() = _start();
                return m_ref();
            }

            return (((tsc - m_tscbase()) * m_scale()) >> 24) + m_ref();
        }

        void start()
        {
            m_start = _start();
        }

        void stop()
        {
            m_end = _stop();
        }

        uint64_t getElapsedTicks() { return m_end - m_start; }
        uint64_t getElapsedTime() { return ((m_end - m_start) * m_scale()) >> 24; }
    };
    /*
      uint64_t ITime_RDTSC::m_tscbase;
      uint64_t ITime_RDTSC::m_ref;
      uint64_t ITime_RDTSC::m_scale;
      */
    /*---------------------------------------------------------------------------
     * ITime_Logger
     * Util for logger time
     *---------------------------------------------------------------------------
     */
    class ITime_Logger
    {
    public:
        /*!
         * @brief given the milliseconds from midnight, the method returns milliseconds calculated from epock in local time zone.
         * @param milliSecFromMidnight milliseconds elapsed from midnight
         * @return milliseconds from epoch in local time zones
         */
        static time_t GetTimeFromEpochGivenMilliSecFromMidNight(time_t milliSecFromMidnight)
        {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            time_t localTimeZoneSeconds = ts.tv_sec;
            struct tm t;
            localtime_r(&localTimeZoneSeconds, &t);
            // set the time to 00:00:00 (midnight)
            t.tm_hour = 0;
            t.tm_sec = 0;
            t.tm_min = 0;
            time_t secondsFromEpockTillMidNite = mktime(&t);

            time_t milliSecondsSoFar = (secondsFromEpockTillMidNite * 1000) + milliSecFromMidnight;

            return milliSecondsSoFar;
        }

        //*****************************************************
        // ConvertYYYYMMDDhhmmss_ns - nanoseconds, copy
        //*****************************************************
        static std::string ConvertYYYYMMDDhhmmss_ns(uint64_t intime, bool usegmtime = false)
        {
            std::string out;
            return ConvertYYYYMMDDhhmmss_ns(intime, out, usegmtime);
        }
        //*****************************************************
        // ConvertYYYYMMDDhhmmss_ns - nanoseconds, without copy
        //*****************************************************
        static std::string &ConvertYYYYMMDDhhmmss_ns(uint64_t intime, std::string &out, bool usegmtime = false)
        {
            char buff[50];
            memset(buff, '\0', sizeof(buff));

            std::string s_format = "%4d%02d%02d-%02d:%02d:%02d.%09d";
            int nano = intime % 1000000000LL;
            time_t epoch = (time_t)intime / 1000000000LL;

            struct tm t;
            if (usegmtime)
            {
                gmtime_r(&epoch, &t);
            }
            else
            {
                localtime_r(&epoch, &t);
            }
            sprintf(buff, s_format.c_str(),
                    t.tm_year + 1900,
                    t.tm_mon + 1,
                    t.tm_mday,
                    t.tm_hour,
                    t.tm_min,
                    t.tm_sec,
                    nano);

            out = buff;
            return out;
        }

        //*****************************************************
        // ConvertYYYYMMDDhhmmss_us
        //*****************************************************
        static std::string ConvertYYYYMMDDhhmmss_us(uint64_t intime, bool usegmtime = false)
        {
            char buff[50];
            memset(buff, '\0', sizeof(buff));

            std::string s_format = "%4d%02d%02d-%02d:%02d:%02d.%06d";
            int nano = intime % 1000000000LL;
            int micro = nano / 1000;
            time_t epoch = (time_t)intime / 1000000000LL;

            struct tm t;
            if (usegmtime)
            {
                gmtime_r(&epoch, &t);
            }
            else
            {
                localtime_r(&epoch, &t);
            }
            sprintf(buff, s_format.c_str(),
                    t.tm_year + 1900,
                    t.tm_mon + 1,
                    t.tm_mday,
                    t.tm_hour,
                    t.tm_min,
                    t.tm_sec,
                    micro);

            return (std::string(buff));
        }

        //*****************************************************
        // GetCurrentYYYYMMDDhhmmss
        //*****************************************************
        static std::string GetCurrentYYYYMMDDhhmmss()
        {
            uint64_t intime = ITime_ClockRT::GetCurrentTime();

            char buff[50];
            memset(buff, '\0', sizeof(buff));

            std::string s_format = "%4d%02d%02d-%02d.%02d.%02d";
            time_t epoch = (time_t)intime / 1000000000LL;

            struct tm t;
            localtime_r(&epoch, &t);
            sprintf(buff, s_format.c_str(),
                    t.tm_year + 1900,
                    t.tm_mon + 1,
                    t.tm_mday,
                    t.tm_hour,
                    t.tm_min,
                    t.tm_sec);
            return (std::string(buff));
        }

        static std::string GetCurrentYYYYMMDDhhmmss(uint64_t intime)
        {
            char buff[50];
            std::string s_format = "%4d%02d%02d-%02d.%02d.%02d";
            time_t epoch = (time_t)intime / 1000000000LL;

            struct tm t;
            localtime_r(&epoch, &t);
            sprintf(buff, s_format.c_str(),
                    t.tm_year + 1900,
                    t.tm_mon + 1,
                    t.tm_mday,
                    t.tm_hour,
                    t.tm_min,
                    t.tm_sec);
            return (std::string(buff));
        }

        //*****************************************************
        // GetCurrentYYYYMMDD
        //*****************************************************
        static std::string GetCurrentYYYYMMDD()
        {
            uint64_t intime = ITime_ClockRT::GetCurrentTime();

            char buff[50];
            memset(buff, '\0', sizeof(buff));

            std::string s_format = "%4d%02d%02d";

            time_t epoch = (time_t)intime / 1000000000LL;

            struct tm t;
            localtime_r(&epoch, &t);
            sprintf(buff, s_format.c_str(),
                    t.tm_year + 1900,
                    t.tm_mon + 1,
                    t.tm_mday);
            return (std::string(buff));
        }

        //*****************************************************
        // GetCurrentYYYYMMDDFromLong
        //*****************************************************
        static std::string GetCurrentYYYYMMDDFromLong(uint64_t intime)
        {
            char buff[50];
            memset(buff, '\0', sizeof(buff));

            std::string s_format = "%4d%02d%02d";

            time_t epoch = (time_t)intime / 1000000000LL;

            struct tm t;
            localtime_r(&epoch, &t);
            sprintf(buff, s_format.c_str(),
                    t.tm_year + 1900,
                    t.tm_mon + 1,
                    t.tm_mday);
            return (std::string(buff));
        }
    };

    /*---------------------------------------------------------------------------
     * ITime_Convert
     * Time convert helper routines
     *---------------------------------------------------------------------------
     */
    class ITime_Convert
    {

        static const time_t TIME_ELAP_FROM1970TO1980 = 315513000;

    public:
        //*************************************************************
        // convert1980TimeToEpochAscTime
        //*************************************************************
        static char *convert1980TimeToEpochAscTime(time_t time, time_t epoch = TIME_ELAP_FROM1970TO1980)
        {
            time_t inputTime = time + epoch;
            struct tm *timeinfo = localtime(&inputTime);
            return asctime(timeinfo);
        }
        //*************************************************************
        // convertTotime_tAddEpoch
        // input is the number seconds elapsed since 1/1/1980.
        // Adding to it the seconds elapsed from 1/1/1970.
        //*************************************************************
        static time_t convertTotime_tAddEpoch(const std::string &input)
        {
            return (std::stoull(input) + TIME_ELAP_FROM1970TO1980);
        }

        //*************************************************************
        // Www_Mmm_dd_hh_mm_ss_IST_yyyy_To_t
        // Convertion for "dd-Mmm-yyyy hh:mm:ss.mi " format
        //*************************************************************
        static uint64_t dd_Mmm_yyyy_hh_mm_ss_si_To_t(const std::string &x)
        {
            struct tm cal = {0, 0, 0, 0, 0, 0, 0, 0, -1, 0, NULL};
            std::string secTime = x.substr(0, x.rfind("."));
            strptime(secTime.c_str(), "%d-%b-%Y %T", &cal);
            time_t value = mktime(&cal);
            value = value * 1000 + std::stoull(x.substr(x.rfind(".") + 1));
            return value;
        }

        static uint64_t YYYYMMDDHHMM_to_ns(const std::string &s)
        {
            std::cout << s;
            struct tm cal = {0, 0, 0, 0, 0, 0, 0, 0, -1, 0, NULL};
            strptime(s.c_str(), "%Y%m%d%H%M", &cal);
            return illuminati::infra::mktime_internal(&cal) * 1000 * 1000 * 1000;
        }

        //*************************************************************
        // Www_Mmm_dd_hh_mm_ss_IST_yyyy_To_t
        // Convertion for "Www Mmm dd hh:mm:ss IST yyyy" format
        //*************************************************************
        static time_t Www_Mmm_dd_hh_mm_ss_IST_yyyy_To_t(const std::string &x)
        {
            struct tm cal = {0, 0, 0, 0, 0, 0, 0, 0, -1, 0, NULL};
            strptime(x.c_str(), "%A %b %d %H:%M:%S IST %Y", &cal);
            return mktime(&cal);
        }

        //*************************************************************
        // Www_Mmm_dd_hh_mm_ss_IST_yyyy_To_t
        // Convertion for "Www Mmm dd hh:mm:ss IST yyyy" format
        //*************************************************************
        static uint64_t Www_Mmm_dd_hh_mm_ss_IST_yyyy_To_1980_Millsec(const std::string &x)
        {
            struct tm cal = {0, 0, 0, 0, 0, 0, 0, 0, -1, 0, NULL};
            strptime(x.c_str(), "%A %b %d %H:%M:%S IST %Y", &cal);
            uint64_t retTime = mktime(&cal);
            retTime = (retTime - TIME_ELAP_FROM1970TO1980) * 1000;
            return retTime;
        }

        inline __attribute__((always_inline)) static uint64_t HHMMSSToEpochNano(int hh, int mm, int ss)
        {
            time_t rawtime;
            struct tm tm;

            time(&rawtime);
            gmtime_r(&rawtime, &tm);
            tm.tm_hour = hh;
            tm.tm_min = mm;
            tm.tm_sec = ss;
            uint64_t epochTime = infra::mktime_internal(&tm, 0);
            return epochTime * 1000000000;
        }

        static void ConvertYYYYMMDDHHMMSSsss(uint64_t intime, char *output, bool usegmtime = false)
        {
            std::string s_format = "%4d%02d%02d%02d%02d%02d%03d";
            int nano = intime % 1000000000LL;
            int micro = nano / 1000;
            int milli = micro / 1000;
            time_t epoch = (time_t)intime / 1000000000LL;

            struct tm t;
            if (usegmtime)
            {
                gmtime_r(&epoch, &t);
            }
            else
            {
                localtime_r(&epoch, &t);
            }
            sprintf(output, s_format.c_str(),
                    t.tm_year + 1900,
                    t.tm_mon + 1,
                    t.tm_mday,
                    t.tm_hour,
                    t.tm_min,
                    t.tm_sec,
                    milli);
        }

        static void ConvertHHMMSSsss(uint64_t intime, char *output, bool usegmtime = false)
        {
            std::string s_format = "%02d%02d%02d%03d"; // HHMMSSsss
            int nano = intime % 1000000000LL;
            int micro = nano / 1000;
            int milli = micro / 1000;
            time_t epoch = (time_t)intime / 1000000000LL;

            struct tm t;
            if (usegmtime)
            {
                gmtime_r(&epoch, &t);
            }
            else
            {
                localtime_r(&epoch, &t);
            }

            sprintf(output, s_format.c_str(),
                    t.tm_hour,
                    t.tm_min,
                    t.tm_sec,
                    milli);
        }
    };
} // namespace
#endif // ITIMER_H
