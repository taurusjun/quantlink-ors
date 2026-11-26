#ifndef _MmapHandler_H
#define _MmapHandler_H

#include <sys/mman.h>
#include <iostream>
#include <string>
#include "snappy.h"
#include <string.h>

namespace illuminati
{
    namespace io
    {
        // Pankaj Jindal
        static const uint64_t DataCacheSize = 5 * 1024L * 1024L; // 5 * MB
        //

        /*!
         * Performs file operations using memory mapping. The specified file is mapped to virtual memory and requested operations will be performed.
         * Supports anonymous memory mapping, in which case no files will be loaded but the contents will be present in virtual memory.
         * Allows to create block reading, which can be used to read specific blocks of memory parallely using seperate threads.
         */
        class MmapHandler
        {
        private:
            char *startAddress;
            size_t currentIndex;
            size_t mappingSize;

            // Pankaj Jindal
            size_t mappingFilled;
            char *dataCache;
            char *dataCompressedBuffer;
            uint64_t dataCacheIndex;
            size_t packetCount;
            //
            bool m_hasCaches;

        public:
            MmapHandler(bool caches = true) : m_hasCaches(caches)
            {
                currentIndex = 0;
                mappingSize = 0;

                // Pankaj Jindal
                if (caches)
                {
                    dataCache = new char[DataCacheSize];
                    dataCompressedBuffer = new char[DataCacheSize];
                }
                dataCacheIndex = 0;
                packetCount = 0;
                mappingFilled = 0;
                //
            }

            MmapHandler &operator=(const MmapHandler &handler)
            {
                if (&handler == this)
                {
                    return *this;
                }
                // std::cout << "Assignment operator called\n";
                startAddress = handler.startAddress;
                currentIndex = handler.currentIndex;
                mappingSize = handler.mappingSize;

                // Pankaj Jindal
                // dataCache = new char[DataCacheSize];
                // dataCompressedBuffer = new char[DataCacheSize];

                if (m_hasCaches)
                {
                    if (handler.m_hasCaches)
                    {
                        memcpy(dataCache, handler.dataCache, DataCacheSize);
                        memcpy(dataCompressedBuffer, handler.dataCompressedBuffer, DataCacheSize);
                    }
                    else
                    {
                        m_hasCaches = false;
                        delete dataCache;
                        delete dataCompressedBuffer;
                    }
                }
                else
                {
                    if (handler.m_hasCaches)
                    {
                        m_hasCaches = true;
                        dataCache = new char[DataCacheSize];
                        dataCompressedBuffer = new char[DataCacheSize];
                        memcpy(dataCache, handler.dataCache, DataCacheSize);
                        memcpy(dataCompressedBuffer, handler.dataCompressedBuffer, DataCacheSize);
                    }
                }
                dataCacheIndex = handler.dataCacheIndex;
                packetCount = handler.packetCount;
                mappingFilled = handler.mappingFilled;
                //
                return *this;
            }

            MmapHandler(const MmapHandler &handler)
            {
                startAddress = handler.startAddress;
                currentIndex = handler.currentIndex;
                mappingSize = handler.mappingSize;

                m_hasCaches = handler.m_hasCaches;

                if (m_hasCaches)
                {
                    dataCache = new char[DataCacheSize];
                    dataCompressedBuffer = new char[DataCacheSize];
                    memcpy(dataCache, handler.dataCache, DataCacheSize);
                    memcpy(dataCompressedBuffer, handler.dataCompressedBuffer, DataCacheSize);
                }

                dataCacheIndex = handler.dataCacheIndex;
                packetCount = handler.packetCount;
                mappingFilled = handler.mappingFilled;
                //
            }
            /*!
             * Memory map the file specified by @c filePath.
             * @param filePath Complete path of the file to be memory mapped.
             * @param startOffset Starting offset in the file after which file will be loaded to memory.
             * Default value is @c 0
             * @param size Will load @c size number of bytes to memory. If @c size is 0, will load the rest of the contents of the file.
             * Default value is @c 0
             * @param readOnly If set to true, creates a readonly mapping.
             */
            bool createMapping(const std::string &filePath, off64_t startOffset = 0, size_t size = 0, bool readOnly = true);
            bool createMapping(int fd, off64_t startOffset, size_t size = 0, int prot = PROT_READ, int flags = MAP_SHARED);
            /*!
             * Creates anonymous memory mapping, with the specified size.
             */
            bool createMapping(size_t size);
            size_t read(void *buffer, size_t size);
            size_t read(void *buffer, size_t size, size_t offset);
            size_t read_zc(void **buffer, size_t size, size_t offset);
            void *getPtr(size_t offset);
            size_t write(void *buffer, size_t size);
            size_t write(void *buffer, size_t size, size_t offset);
            void copyContents(const MmapHandler &handler);
            // Start: Pankaj Jindal
            size_t compCache();
            // End: Pankaj Jindal
            void seek(off64_t offset)
            {
                if (offset >= (off64_t)mappingSize)
                    offset = mappingSize - 1;
                currentIndex = offset;
            }
            void reset()
            {
                currentIndex = 0;
            }
            size_t getCurrentIndex()
            {
                return currentIndex;
            }
            size_t getMappingSize()
            {
                return mappingSize;
            }
            uint64_t getDataCacheIndex()
            {
                return dataCacheIndex;
            }
            size_t getMappingFilled()
            {
                return mappingFilled;
            }
            bool resize(size_t newSize);
            void flush(int flags = MS_SYNC)
            {
                msync(startAddress, mappingSize, flags);
            }
            void close()
            {
                munmap(startAddress, mappingSize);
            }
            void optimizeForSequentialAccess()
            {
                madvise(startAddress, mappingSize, MADV_SEQUENTIAL);
            }
            void setPagingTech(int flags)
            {
                madvise(startAddress, mappingSize, flags);
            }
        };
    }
}

#endif
