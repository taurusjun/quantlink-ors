/*
 * Company: way2wealth-illuminati
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-illuminati.com/
 *
 */

#include "filefeedreader.h"
#include <iostream>
#include "ReadFileUsingMMAP.h"
#include "itimer.h"
#include <fstream>

template <typename T, typename U>
void illuminati::md::FileFeedReader<T, U>::startReading()
{
    if (filePath.size() == 0)
    {
        std::cout << "File path is not set\n";
        throw "File path is not set";
    }
    if (dataCollection == nullptr)
    {
        std::cout << "Reader collection is not set \n";
        throw "Reader collection is not set";
    }
    ReadFileUsingMMAP mmapReader;
    mmapReader.set_stream(filePath, true, 2);
    done = false;

    size_t bytesRead = 0;
    U data;
    ReadStates states;
    char buffer[sizeof(U) * 10000];

    ITime_HighResolution timer;
    timer.start();

    // std::ofstream ofs_test("test_nifty");
    while (!abort)
    {
        bytesRead = 0;
        states = mmapReader.stream_read((char *)&buffer, sizeof(U) * 10000, bytesRead);
        char *offset = buffer;
        while (offset - buffer < bytesRead)
        {
            if (bytesRead - (size_t)offset + (size_t)buffer < sizeof(U))
            {
                break;
            }
            memcpy(&data, offset, sizeof(U));
            offset += sizeof(U);
            while (dataCollection->isFull())
            {
                // std::cout<<"writer is full, waiting for reader ...: File = "<<filePath<<std::endl;
                std::this_thread::yield();
            }
            dataCollection->enqueue(data);
            // ofs_test.write((char*)(&data), sizeof(U));
        }
        if (states == FILE_EOF)
            break;
    }
    timer.stop();
    std::cout << "Done with file reading " << filePath << " ...\n";
    std::cout << "\nStatistics for " << filePath << ":\n**********************\n";
    std::cout << "Total time: " << timer.getelapsedMicroSecs() << " (" << timer.getelapsedMicroSecs() / 1000000 << " secs)\n";
    done = true;
    mmapReader.stream_close();

    // ofs_test.close();
}

template <typename T, typename U>
void illuminati::md::FileFeedReader<T, U>::startReadingAsync()
{
    abort = false;
    readerThread = std::thread(&FileFeedReader<T, U>::startReading, this);
}

template <typename T, typename U>
void illuminati::md::FileFeedReader<T, U>::waitForCompletion()
{
    if (readerThread.joinable())
        readerThread.join();
}
