#ifndef __BLOCK_FILE_Reader_H
#define __BLOCK_FILE_Reader_H

#include "MmapHandler.h"
#include "c11compatible.h"
#include "ReadStates.h"

namespace illuminati
{
    namespace io
    {
        class BlockFileHandler
        {
        private:
            uint64_t blockStartIndex;
            /*!
            End index of the block, which is non inclusive.
            Block is defined by [blockStartIndex, blockEndIndex).
             */
            uint64_t blockEndIndex;
            size_t currentIndex;
            MmapHandler *fileHandler;

        public:
            BlockFileHandler() : blockStartIndex(0), blockEndIndex(0), currentIndex(0), fileHandler(nullptr)
            {
            }
            BlockFileHandler(MmapHandler *fh, uint64_t bsi, uint64_t bei) : blockStartIndex(bsi), blockEndIndex(bei), currentIndex(0), fileHandler(fh)
            {
            }
            size_t getSize()
            {
                return blockEndIndex - blockStartIndex;
            }
            uint64_t getCurrentPos()
            {
                return currentIndex;
            }
            void reset()
            {
                currentIndex = 0;
            }
            void seek(size_t index)
            {
                if (currentIndex + blockStartIndex >= blockEndIndex)
                    currentIndex = blockEndIndex - 1;
                else
                    currentIndex = index;
            }
            void setHandler(MmapHandler *handler)
            {
                fileHandler = handler;
            }
            void setBlock(uint64_t blockStartIndex, uint64_t blockEndIndex)
            {
                this->blockStartIndex = blockStartIndex;
                this->blockEndIndex = blockEndIndex;
            }
            void set_stream(std::string filePath);
            size_t write(void *buffer, size_t size);
            size_t read(void *buffer, size_t size);
            size_t read_zc(void **buffer, size_t size);
            ReadStates stream_read(char *buffer, size_t size, size_t &bytesRead);
            ReadStates stream_read_zc(char **buffer, size_t size, size_t &bytesRead);

            void *beginRead();
            void endRead(size_t bytesRead);
        };
    }
}

#endif
