/*
 * Company: way2wealth-illuminati
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-illuminati.com/
 *
 */

#ifndef SNAPPYDECOMPRESSOR_H
#define SNAPPYDECOMPRESSOR_H

#include "snappy.h"
#include "constants.h"
#include <fstream>
#include <string>
#include <iostream>
#include <cstring>
#include "c11compatible.h"

namespace illuminati
{
    template <size_t MaxSize = 10 * MB>
    class SnappyDecompressor
    {
    private:
        char buffer[MaxSize == 0 ? 10 * MB : 2 * MaxSize];
        // TODO: Ideally compressed cache should be smaller than the actual data cache.
        char bufferCompressed[MaxSize == 0 ? 10 * MB : 2 * MaxSize];
        uint64_t bufferIndex;
        uint64_t decompressedLength;
        std::ifstream compressedDump;

    public:
        SnappyDecompressor()
        {
            bufferIndex = 0;
            decompressedLength = 0;
        }
        ~SnappyDecompressor()
        {
            compressedDump.close();
        }

        void setDump(std::string filename)
        {
            if (compressedDump.is_open())
                compressedDump.close();
            compressedDump.open(filename);
            if (!compressedDump.good())
            {
                std::cerr << "Unable to open dump file: " << filename << "\n";
                throw std::string("Unable to open dump file.");
            }
        }

        template <typename T>
        bool decompress(T &data)
        {
            size_t decompressedSize = 0;
            if (!decompress((char *)&data, sizeof(T), decompressedSize))
            {
                if (decompressedSize != 0)
                {
                    throw std::string("Decompression error: few bytes are left");
                }
                return false;
            }
            return true;
        }

        bool decompress(char *dataBuffer, size_t length, size_t &decompressedSize)
        {
            if (bufferIndex + length > decompressedLength)
            {
                // decompress data to cache.
                if (bufferIndex < decompressedLength)
                    memmove(buffer, buffer + bufferIndex, decompressedLength - bufferIndex);
                uint64_t packetLength;
                const char *packet = decompress(packetLength);
                if (packet == nullptr)
                {
                    decompressedSize = decompressedLength - bufferIndex;
                    return false;
                }
                memcpy(buffer + decompressedLength - bufferIndex, packet, packetLength);
                decompressedLength = packetLength + (decompressedLength - bufferIndex);
                bufferIndex = 0;
            }
            memcpy(dataBuffer, buffer + bufferIndex, length);
            bufferIndex += length;
            decompressedSize = length;
            return true;
        }

        // returns a pointer to preallocated buffer. If any changes has to be made on data, a copy has to be made.
        // returns nullptr on EOF
        const char *decompress(uint64_t &uncompressedLength)
        {
            if (compressedDump.eof())
                return nullptr;
            size_t compressedLength;
            compressedDump.read((char *)&compressedLength, sizeof(compressedLength));
            compressedDump.read(bufferCompressed, compressedLength);
            if (compressedDump.gcount() > 0)
            {
                snappy::RawUncompress(bufferCompressed, compressedLength, buffer);
                snappy::GetUncompressedLength(bufferCompressed, compressedLength, &uncompressedLength);
                const char *data = const_cast<char *>(buffer);
                return data;
            }
            return nullptr;
        }
    };
}

#endif // SNAPPYDECOMPRESSOR_H
