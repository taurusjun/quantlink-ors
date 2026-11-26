#ifndef _WRITEFILEUSINGSTREAM_H
#define _WRITEFILEUSINGSTREAM_H

#include <string>
#include <fstream>
#include <cstring>
#include <iostream>
#include <cstddef>
#include <unordered_set>
#include <unordered_map>
#include <cstddef>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "marketdelta.h"
#include "WriteStates.h"

using illuminati::infra::Header;

using namespace std;

class WriteFileUsingStreams
{
public:
    WriteFileUsingStreams()
    {
        m_DEBUG = false;
    }

    bool set_file(const string &filePath_, unordered_set<string> &symbols_)
    {
        if (symbols_.size() == 0)
        {
            throw string("WriteFileUsingStreams: NO symbol list passed");
        }

        unordered_set<string>::iterator it = symbols_.begin();
        while (it != symbols_.end())
        {
            string full_filepath = filePath_ + (*it) + ".dat";
            cout << "File for writing is " << full_filepath << endl;
            m_writeStreams.insert(pair<string, string>((*it), full_filepath));
            it++;
        }
    }

    WriteStates stream_write_select(Header *data, size_t bytes_to_write)
    {
        unordered_map<string, string>::iterator it = m_writeStreams.find(data->Symbol);
        if (it != m_writeStreams.end())
        {
            if (!m_DEBUG)
            {
                ofstream write_stream(it->second.c_str(), ofstream::out | ofstream::app | ofstream::binary);
                if (write_stream.is_open())
                {
                    write_stream.write((char *)data, bytes_to_write);
                    write_stream.clear();
                    write_stream.close();
                }
                else
                {
                    throw string("ERROR: Opening file ") + string(it->second.c_str());
                }
            }
            else
            {
                // cout<<data->toString();
                // cout<<" TO "<<it->second<<endl;
            }
        }
    }

    WriteStates stream_write_all(Header *data, size_t bytes_to_write)
    {
        unordered_map<string, string>::iterator it = m_writeStreams.begin();
        while (it != m_writeStreams.end())
        {
            if (!m_DEBUG)
            {
                ofstream write_stream(it->second.c_str(), ofstream::out | ofstream::app | ofstream::binary);
                write_stream.write((char *)data, bytes_to_write);
                write_stream.clear();
                write_stream.close();
            }
            else
            {
                /*
                if(data->Type == EndPkt )
                {
                    cout<<"EndPkt for "<<it->second<<endl;
                }
                else
                {
                    cout<<"ERROR: write all called for wrong data "<<endl;
                }*/
            }
            it++;
        }
    }

    int64_t stream_bytes()
    {
        unordered_map<string, string>::iterator it = m_writeStreams.begin();
        int64_t total_size = 0;
        while (it != m_writeStreams.end())
        {
            struct stat sb;
            int32_t fd = open(it->second.c_str(), O_RDONLY);
            if (fd == -1)
            {
                throw string("ERROR: unable to open file : " + it->second);
            }
            if (fstat(fd, &sb) == -1)
            {
                throw string("ERROR: fstat failed");
            }
            total_size += sb.st_size;
            it++;
        }

        return total_size;
    }

private:
    unordered_map<string, string> m_writeStreams;
    bool m_DEBUG;
};

#endif
