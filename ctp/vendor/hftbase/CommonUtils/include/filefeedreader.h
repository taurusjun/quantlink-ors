/*
 * Company: way2wealth-illuminati
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-illuminati.com/
 *
 */

#ifndef FILEFEEDREADER_H
#define FILEFEEDREADER_H

#include <string>
#include <thread>

namespace illuminati
{
    namespace md
    {
        /*!
         * @tparam T Type of the data structure where the read structures will be inserted.
         * @tparam U Type of the structure which represents the contents of the file.
         */
        template <typename T, typename U>
        class FileFeedReader
        {
        private:
            std::string filePath;
            std::thread readerThread;
            T *dataCollection;
            bool done;
            bool abort;

        public:
            FileFeedReader() : filePath(""), dataCollection(nullptr), done(false), abort(false) {}
            void setFilePath(std::string filePath)
            {
                this->filePath = filePath;
            }
            void startReadingAsync();
            void startReading();
            void waitForCompletion();
            void setDataCollection(T *dataCollection)
            {
                this->dataCollection = dataCollection;
            }
            void removeDataCollector()
            {
                delete dataCollection;
            }
            bool hasCompleted()
            {
                return done;
            }
            void stop()
            {
                abort = true;
            }
        };
    }
}

#endif // FILEFEEDREADER_H
