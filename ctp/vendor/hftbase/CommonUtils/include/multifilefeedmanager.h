/*
 * Company: way2wealth-illuminati
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-illuminati.com/
 *
 */

#ifndef MULTIFILEFEEDMANAGER_H
#define MULTIFILEFEEDMANAGER_H

#include "multifilefeedreader.h"
#include <vector>
#include <thread>

namespace illuminati
{
    namespace md
    {
        template <typename T>
        struct FeederInfo
        {
            std::string filePath;
            T *dataCollector;
        };
        template <typename T, typename U>
        class MultiFileFeedManager
        {
        private:
            std::vector<MultiFileFeedReader<T, U> *> feeders;
            std::vector<FeederInfo<T>> readerInfo;
            int threadCount;

        public:
            MultiFileFeedManager() : threadCount(10) {}
            ~MultiFileFeedManager();
            void addFile(std::string, T *);
            void setThreadCount(int threadCount)
            {
                this->threadCount = threadCount;
            }
            void start();
            void waitForCompletion();
            void abort();
            FileReaderInfo<T> *getFeeder(int index);
        };
    }
}

#endif // MULTIFILEFEEDMANAGER_H
