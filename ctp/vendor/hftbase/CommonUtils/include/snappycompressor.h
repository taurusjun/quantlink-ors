/*
 * Company: way2wealth-illuminati
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-illuminati.com/
 *
 */

#ifndef SNAPPYCOMPRESSOR_H
#define SNAPPYCOMPRESSOR_H

#include "snappy.h"
#include "constants.h"
#include <fstream>
#include <string>
#include <iostream>
#include <cstring>

namespace illuminati
{
    template <size_t MaxSize = 10 * MB>
    class SnappyCompressor
    {
    private:
        char cache[MaxSize == 0 ? 10 * MB : MaxSize];
        // TODO: Ideally compressed cache should be smaller than the actual data cache.
        char cacheCompressed[MaxSize == 0 ? 10 * MB : MaxSize];

        uint64_t cacheIndex;
        std::ofstream compressedDump;

        uint64_t totalCalls;
        uint64_t totalSize;

    public:
        SnappyCompressor()
        {
            cacheIndex = 0;

            totalCalls = 0;
            totalSize = 0;
        }

        ~SnappyCompressor()
        {
            compressedDump.close();
            std::cout << "Total calls: " << totalCalls << " : Total size : " << totalSize << "\n";
        }

        void setDump(std::string filename)
        {
            if (compressedDump.is_open())
                compressedDump.close();
            compressedDump.open(filename);
            if (!compressedDump.good())
            {
                std::cerr << "Unable to open dump file: " << filename << "\n";
                throw "Unable to open dump file.";
            }
        }

        template <typename T>
        void compress(const T &data)
        {
            compress((const char *)&data, sizeof(T));
        }

        void compress(const char *buffer, size_t length)
        {
            totalCalls++;
            if (cacheIndex + length > MaxSize)
            {
                flush();
                cacheIndex = 0;
            }
            memcpy(cache + cacheIndex, buffer, length);
            cacheIndex += length;
        }

        void flush()
        {
            if (cacheIndex > 0)
            {
                size_t compressedLength;
                snappy::RawCompress(cache, cacheIndex, cacheCompressed, &compressedLength);
                compressedDump.write((const char *)&compressedLength, sizeof(compressedLength));
                compressedDump.write(cacheCompressed, compressedLength);

                totalSize += compressedLength;
                cacheIndex = 0;
            }
        }
    };
}

#endif // SNAPPYCOMPRESSOR_H
