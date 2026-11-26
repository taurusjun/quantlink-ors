#ifndef _STATISTICS_H
#define _STATISTICS_H
#include <vector>
#include <algorithm>
#include <iomanip>

namespace illuminati
{
    namespace statistics
    {

        template <typename T>
        static double mean(std::vector<T> &v)
        {
            T total = 0;
            for (int i = 0; i < v.size(); i++)
            {
                total += v[i];
            }
            return ((double)total) / ((double)v.size());
        }

        template <typename T>
        static T median(std::vector<T> &v)
        {
            std::sort(v.begin(), v.end());
            return v[v.size() / 2];
        }

        template <typename T>
        static std::vector<T> percentiles(std::vector<T> &v)
        {
            std::sort(v.begin(), v.end());
            std::vector<T> perc;

            for (int i = 1; i < 10; i++)
            {
                perc.push_back(v[(v.size() * i * 10) / 100]);
            }
            perc.push_back(v[v.size() - 1]);

            return perc;
        }

        template <typename T>
        static T min(std::vector<T> &v)
        {
            std::sort(v.begin(), v.end());
            return v[0];
        }

        template <typename T>
        static T max(std::vector<T> &v)
        {
            std::sort(v.begin(), v.end());
            return v[v.size() - 1];
        }

        template <typename T>
        static void printAllStats(std::vector<T> &v)
        {
            std::cout << std::setprecision(20);
            std::cout << "Count: " << v.size() << "\n";
            std::cout << "Mean: " << statistics::mean(v) << "\n";
            std::cout << "Median: " << statistics::median(v) << "\n";
            std::cout << "Min: " << statistics::min(v) << "\n";
            std::cout << "Max: " << statistics::max(v) << "\n";

            std::cout << "Percentiles: \n";
            std::vector<T> perc = statistics::percentiles(v);
            for (int i = 0; i < perc.size(); i++)
            {
                std::cout << " " << (i + 1) * 10 << "% : " << perc[i] << "\n";
            }
        }
    } // namespace statistics
}

#endif
