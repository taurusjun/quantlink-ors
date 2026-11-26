
/*
 * Company: way2wealth-illuminati
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-illuminati.com/
 *
 */

#ifndef MULTIFILEFEEDREADER_H
#define MULTIFILEFEEDREADER_H

#include "ReadFileUsingMMAP.h"
#include <string>
#include <stdint.h>
#include <thread>
#include "c11compatible.h"
#include "orderresponse.h"

namespace illuminati
{
    namespace md
    {
        template <typename T>
        struct FileReaderInfo
        {
            ReadFileUsingMMAP reader;
            T *dataCollector;
            bool completed;
            std::string filePath;
            illuminati::infra::ExchangeType exchangeIdentifier;
            FileReaderInfo()
            {
                dataCollector = nullptr;
                completed = false;
            }
            bool hasCompleted()
            {
                return completed;
            }
        };
        /*!
         * @tparam T Type of the data structure where the read structures will be inserted.
         * @tparam U Type of the structure which represents the contents of the file.
         */
        template <typename T, typename U>
        class MultiFileFeedReader
        {
        private:
            std::vector<FileReaderInfo<T>> fileReader;
            int filePathIndex;
            uint64_t chunkSize;
            int readFile(int index, int64_t size);
            std::thread threadId;

            static int testId;
            int myId;
            bool bcompleted;
            bool forcedExit;
            char buffer[sizeof(U) * 10000];

        public:
            MultiFileFeedReader() : filePathIndex(0), chunkSize(sizeof(U) * 100), bcompleted(false), forcedExit(false) { myId = testId++; }
            void addFile(std::string filePath, T *);
            void start();
            void setChunkSize(uint64_t chunkSize)
            {
                this->chunkSize = chunkSize;
            }
            void waitForCompletion();
            void startAsync();
            bool hasCompleted()
            {
                return bcompleted;
            }
            FileReaderInfo<T> *getFileReader(int index)
            {
                return &(fileReader[index]);
            }
            void abort();
        };
    }
}

#endif // MULTIFILEFEEDREADER_H
