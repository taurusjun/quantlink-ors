/*
 * mktime_internal.h
 */

#ifndef MKTIME_INTERNAL_H_
#define MKTIME_INTERNAL_H_
namespace illuminati
{
    namespace infra
    {
        /*!
         * Fast mktime
         * Note: this func is not tz aware hence its not a replacement of mktime c function.
         * However you can pass in the utcdiff (utc offset) for your timezone. Please
         * test the results around DST changes if you pass in utcdiffs
         * Currenlty the leaps calculation works till 2100 - to make it faster
         * i.e:
         *    as 1972 is leap and then its every 4 years, so till 2100.01.01 we can do this:
         *     leaps = (tyears + 2) >> 2
         * the leap calculation ideally is (add a new function if you need this calc):
         *    i = (ltm->tm_year – 100) / 100;
         *    leaps -= ( (i/4)*3 + i%4 );
         * There are no validations here to avoid branching, make sure that you pass
         * in correct struct tm values.
         */
        // use days lookup instead of looping for days
        const static int MK_DAYS_TABLE[] =
            {
                0,
                31,
                31 + 28,
                31 + 28 + 31,
                31 + 28 + 31 + 30,
                31 + 28 + 31 + 30 + 31,
                31 + 28 + 31 + 30 + 31 + 30,
                31 + 28 + 31 + 30 + 31 + 30 + 31,
                31 + 28 + 31 + 30 + 31 + 30 + 31 + 31,
                31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30,
                31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31,
                31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30};

        inline __attribute__((always_inline)) static time_t mktime_internal(const struct tm *ltm, int utcdiff = 0)
        {
            int tyears, tdays, leaps, utc_hrs;
            // tm->tm_year is from 1900, here we are calculating unix-epoch
            tyears = ltm->tm_year - 70;
            leaps = (tyears + 2) >> 2; // till 2100

            tdays = MK_DAYS_TABLE[ltm->tm_mon]; // today days till the month
            tdays += ltm->tm_mday - 1;          // days of month passed.
            tdays = tdays + (tyears * 365) + leaps;

            utc_hrs = ltm->tm_hour + utcdiff; // time zone 0 for utc.
            return (tdays * 86400) + (utc_hrs * 3600) + (ltm->tm_min * 60) + ltm->tm_sec;
        }
    }
}
#endif /* MKTIME_INTERNAL_H_ */
