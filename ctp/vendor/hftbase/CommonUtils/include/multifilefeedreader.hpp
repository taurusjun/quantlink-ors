/*
 * Company: way2wealth-illuminati
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-illuminati.com/
 *
 */

#include "multifilefeedreader.h"
#include <thread>
#include <fstream>
#include <sstream>

#include "marketdelta.h"

using namespace illuminati::md;

template <typename T, typename U>
int MultiFileFeedReader<T, U>::testId = 0;

template <typename T, typename U>
void MultiFileFeedReader<T, U>::addFile(std::string filePath, T *dataCollector)
{
    FileReaderInfo<T> info;
    info.filePath = filePath;
    string::size_type pos = filePath.rfind('/');
    string s = filePath.substr(pos + 1);
    pos = s.find('_');
    s = s.substr(0, pos);

    if (strcmp(s.c_str(), "NSECM") == 0)
        info.exchangeIdentifier = illuminati::infra::NSE_CM;
    else if (strcmp(s.c_str(), "NSEFO") == 0)
        info.exchangeIdentifier = illuminati::infra::NSE_FO;
    else if (strcmp(s.c_str(), "NSECDS") == 0)
        info.exchangeIdentifier = illuminati::infra::NSE_CDS;

    info.reader.set_stream(filePath, true, 2);
    info.dataCollector = dataCollector;
    fileReader.push_back(info);

    filePathIndex++;
}

template <typename T, typename U>
void MultiFileFeedReader<T, U>::start()
{
    bcompleted = false;
    bool allCompleted = false;
    bool allFull = true;
    int totalPackets = 0;
    while (!allCompleted && !forcedExit)
    {
        allFull = true;
        allCompleted = true;
        for (int index = 0; index < filePathIndex; index++)
        {
            if (!fileReader[index].completed)
            {
                allCompleted = false;
                if (!fileReader[index].dataCollector->isFull())
                {
                    allFull = false;
                    int64_t size = fileReader[index].dataCollector->getRemainingSize();
                    totalPackets += readFile(index, size);
                }
                /*
                else
                {
                std::cout<<"writer is full, waiting for reader ...\n";
                }
                */
            }
        }
        if (allFull && !allCompleted)
        {
            // std::this_thread::yield();
            usleep(5);
            // std::cout<<"All full and not completed, yielding ...\n";
        }
    }

    // for(int i=0; i<filePathIndex; i++)
    //   std::cout<<"Feed reader for "<<fileReader[i].filePath<<" is done\n";
    std::cout << "Total packets read for " << filePathIndex << " files: " << totalPackets << "\n";

    bcompleted = true;
}

template <typename T, typename U>
int MultiFileFeedReader<T, U>::readFile(int index, int64_t size)
{
    if (size <= 0)
        return 0;
    /*
    std::stringstream testname;
    testname<<"../../../ExchangeSimulator/bin/test_";
    testname<<myId<<"_"<<index;
    std::ofstream ofs_test(testname.str(), ios::binary | ios::app);
    */

    ITime_HighResolution timer;
    size_t bytesRead = 0;
    // TODO: Currently we are dependent on the size of the buffer to be a multiple of sizeof(U).
    // Expect stream_read to return chuncks smaller than this size and perform the knitting of the data.

    if (size > 10000)
        size = 10000;

    // std::cout<<"Reading "<<size<<" packets at a time\n";
    size = sizeof(U) * size;
    char *offset = buffer;
    U data;

    FileReaderInfo<T> &readerInfo = fileReader[index];

    ReadStates states = readerInfo.reader.stream_read((char *)&buffer, size, bytesRead);
    if (states == FILE_EOF)
    {
        readerInfo.completed = true;
        return 0;
    }

    int totalPacketsRead = 0;
    while (offset - buffer < bytesRead)
    {
        if (bytesRead - (size_t)offset + (size_t)buffer < sizeof(U))
        {
            break;
        }
        memcpy(&data, offset, sizeof(U));
        offset += sizeof(U);
        illuminati::infra::HeaderWrap wrap;
        memcpy((char *)&wrap.actualdata, &data, sizeof(U));
        wrap.exchange_type = readerInfo.exchangeIdentifier;

        readerInfo.dataCollector->enqueue(wrap);
        totalPacketsRead++;
        // std::cout<<"enequeud data for \n";
        // ofs_test.write((char*)&data, sizeof(U));
    }
    // ofs_test.close();
    return totalPacketsRead;
}

template <typename T, typename U>
void MultiFileFeedReader<T, U>::startAsync()
{
    threadId = std::thread(&MultiFileFeedReader<T, U>::start, this);
}

template <typename T, typename U>
void MultiFileFeedReader<T, U>::waitForCompletion()
{
    if (threadId.joinable())
        threadId.join();
}

template <typename T, typename U>
void MultiFileFeedReader<T, U>::abort()
{
    forcedExit = true;
}
