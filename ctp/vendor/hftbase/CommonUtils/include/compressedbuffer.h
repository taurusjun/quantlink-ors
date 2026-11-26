#ifndef _COMPRESSED_BUFFER_H
#define _COMPRESSED_BUFFER_H
#include <stdint.h>
#include <cstdlib>

namespace illuminati
{
    namespace ds
    {
        class CompressedBuffer
        {
        protected:
            static const uint64_t InitDataCacheSize = 5 * 1024L * 1024L; // 5 * MB
            char *dataCache;
            size_t dataCacheIndex;
            size_t dataCacheSize;

            size_t packetCount;

            char *buffer;
            size_t bufSize;
            size_t bufIndex;

            void growBuffer();

            void growBuffer(size_t size);

            inline void bufWrite(const char *buf, size_t s);

            void compressCache();

        public:
            CompressedBuffer(){};
            ~CompressedBuffer();

            void init();

            void write(char *buf, size_t size);

            size_t getSize()
            {
                return bufIndex;
            }

            const char *getBuffer()
            {
                return buffer;
            }

            void finalize()
            {
                compressCache();
            }
        };
    } // namespace ds
} // namespace illuminati
#endif
