/*
 * Company: way2wealth-illuminati
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-illuminati.com/
 *
 */

#include "multifilefeedmanager.h"
#include <math.h>
#include <iostream>

using namespace illuminati::md;

template <typename T, typename U>
MultiFileFeedManager<T, U>::~MultiFileFeedManager()
{
    for (int i = 0; i < feeders.size(); i++)
    {
        delete feeders[i];
    }
}

template <typename T, typename U>
void MultiFileFeedManager<T, U>::start()
{
    std::cout << "Number of files=" << readerInfo.size() << " Threadcount=" << threadCount << "\n";
    int feederCount = (readerInfo.size() <= threadCount) ? readerInfo.size() : threadCount;
    int nFiles = ceil((float)readerInfo.size() / feederCount);
    std::cout << "Going to start " << feederCount << " feeders with " << nFiles << " each files to process \n";
    for (int i = 0; i < feederCount; i++)
    {
        if (i * nFiles >= readerInfo.size())
        {
            std::cout << "no need to create any more feeders: Total feeders created: " << feeders.size() << "\n";
            break;
        }
        MultiFileFeedReader<T, U> *reader = new MultiFileFeedReader<T, U>();
        for (int j = 0; j < nFiles; j++)
        {
            if ((i * (nFiles) + j) < readerInfo.size())
            {
                std::cout << "Adding file " << readerInfo[i * (nFiles) + j].filePath << " to the feeder\n";
                reader->addFile(readerInfo[i * (nFiles) + j].filePath, readerInfo[i * (nFiles) + j].dataCollector);
            }
            else
                break;
        }
        reader->startAsync();
        feeders.push_back(reader);
    }
}

template <typename T, typename U>
void MultiFileFeedManager<T, U>::addFile(string filePath, T *dataCollector)
{
    FeederInfo<T> f;
    f.filePath = filePath;
    f.dataCollector = dataCollector;
    readerInfo.push_back(f);
}

template <typename T, typename U>
FileReaderInfo<T> *MultiFileFeedManager<T, U>::getFeeder(int index)
{
    int feederCount = (readerInfo.size() <= threadCount) ? readerInfo.size() : threadCount;
    int nFiles = ceil((float)readerInfo.size() / feederCount);
    int feederIndex = floor((float)index / nFiles);
    // std::cout<<"Retrieving index="<<index<<" feeder index="<<feederIndex<<" reader index="<<index%nFiles<<"\n";
    return feeders[feederIndex]->getFileReader((index % nFiles));
}

template <typename T, typename U>
void MultiFileFeedManager<T, U>::waitForCompletion()
{
    for (int i = 0; i < feeders.size(); i++)
    {
        std::cout << "FeedManager: Waiting for feeder#" << i << "\n";
        feeders[i]->waitForCompletion();
        std::cout << "FeedManager: Completed waiting" << endl;
    }
}

template <typename T, typename U>
void MultiFileFeedManager<T, U>::abort()
{
    for (int i = 0; i < feeders.size(); i++)
    {
        feeders[i]->abort();
    }
}
