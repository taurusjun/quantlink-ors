#ifndef _FILEREADPOLICY_H
#define _FILEREADPOLICY_H

#include <string>
#include <fstream>
#include <cstring>
#include <iostream>
#include <cstddef>

#include "ReadStates.h"

using namespace std;
class ReadFileUsingStreams
{
public:
    ReadFileUsingStreams()
    {
    }

    bool set_stream(const string &filePath_)
    {
        m_readStream.open(filePath_.c_str(), ifstream::in | ifstream::binary);
        if (!m_readStream.is_open())
            throw string("Error: opening file " + filePath_);
        return true;
    }

    ReadStates stream_read(char *buf, int32_t chunk, size_t &bytes_read)
    {
        if (!m_readStream.eof())
        {
            bytes_read = m_readStream.read(buf, chunk).gcount();
            if (m_readStream.rdstate() == ios_base::goodbit)
                return READ_SUCC;
            else if (m_readStream.rdstate() == ios_base::eofbit)
                return FILE_EOF;
            else
                return READ_ERR;
        }
        else
        {
            return FILE_EOF;
        }
    }

    void stream_close()
    {
        if (m_readStream.is_open())
        {
            m_readStream.clear();
            m_readStream.close();
        }
    }

private:
    string m_filePath;
    ifstream m_readStream;
};

#endif
