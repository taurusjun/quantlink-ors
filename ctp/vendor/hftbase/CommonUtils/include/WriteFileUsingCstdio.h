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
#include <stdio.h>
#include <signal.h>
#include <algorithm>

#include "WriteStates.h"
#include "stringutils.h"

using namespace std;

static unordered_map<string, FILE *> m_writeStreams;

template <size_t N, class T>
class WriteFileUsingCstdio
{
public:
    WriteFileUsingCstdio()
        : m_fill(0), m_DEBUG(false)
    {
    }

    bool set_file(const string &filePath_, unordered_set<T> &suffix_, const string &mode = "ab")
    {
        if (suffix_.size() == 0)
        {
            throw string("WriteFileUsingCstdio: NO symbol list passed");
        }

        FILE *fptr;
        string full_filepath = "";
        int32_t index = 0;
        //  if( meta_ )
        //  {
        //    string full_filepath = filePath_ + "METADATA.dat";
        //    if ( ( fptr = fopen ( full_filepath.c_str(), "ab" ) ) == NULL )
        //    {
        //      throw string ( "WriteFileUsingCstdio: Cannot open file for write " ) + full_filepath;
        //    }
        //    cout<<"registering file "<<full_filepath<<" for writes"<<endl;
        //    m_streams[ index ] = fptr; m_names[index] = string( "METADATA" ); index++;
        //  }

        typename unordered_set<T>::iterator it = suffix_.begin();

        while (it != suffix_.end())
        {
            full_filepath = filePath_ + (*it) + ".dat";

            if ((fptr = fopen(full_filepath.c_str(), mode.c_str())) == NULL)
            {
                throw string("WriteFileUsingCstdio: Cannot open file for write ") + full_filepath;
            }
            else
            {
                cout << "registering file " << full_filepath << " for writes" << endl;
                m_streams[index] = fptr;
                m_names[index] = *it;
                index++;
            }

            it++;
        }

        m_fill = index;
        cout << "total symbols registered are " << m_fill << endl;
        return true;
    }

    WriteStates stream_write_select(void *data, size_t bytes_, size_t records_, const T &s_)
    {
        bool status = true;
        int32_t index;

        /*
         * write the specific file
         */
        // cout << "searcing for symbol "<< s_ << endl;
        if ((index = __find(s_)) != -1)
        {
            // cout << "found index "<< index << endl;
            if (fwrite_unlocked(data, bytes_, records_, m_streams[index]) == 0)
            {
                status = false;
            }

            if (status)
                return WRITE_SUCC;
            else
                return WRITE_ERR;
        }
        else
        {
            // cout << "couldn't find symb "<< s_ << endl;
            return WRITE_MISC;
        }
    }

    WriteStates stream_close()
    {
        int32_t fd;

        for (int32_t i = 0; i < m_fill; i++)
        {
            if ((fd = fileno(m_streams[i])) == -1)
            {
                cerr << "Error in retrieving fd for file : " << m_names[i] << ".dat" << "\n";
                cerr << "Consequence -> flush will not work, force closing, may lose some data"
                     << endl;
                fclose(m_streams[i]);
                continue;
            }

            fsync(fd);
            fclose(m_streams[i]);
            cout << "closed the stream " << m_names[i] << "\n";
        }

        return WRITE_SUCC;
    }

    WriteStates stream_write_all(void *data, size_t bytes_, size_t records_)
    {
        bool all_writes_success = true;
        int32_t index = 0;

        while (index <= m_fill - 1)
        {
            if (fwrite_unlocked(data, bytes_, records_, m_streams[index]) == 0)
            {
                all_writes_success = false;
            }

            index++;
        }

        if (all_writes_success)
        {
            return WRITE_SUCC;
        }
        else
        {
            return WRITE_ERR;
        }
    }

    int64_t stream_bytes()
    {
        int32_t index = 0;
        int64_t total_size = 0;

        while (index <= m_fill - 1)
        {
            int32_t fd = fileno(m_streams[index]);
            struct stat sb;

            if (fstat(fd, &sb) == -1)
            {
                throw string("ERROR: fstat failed");
            }

            total_size += sb.st_size;
            index++;
        }

        return total_size;
    }

protected:
    int32_t __find(const T &search_)
    {
        T *pos = find(&m_names[0], &m_names[m_fill], search_);

        if (pos != &m_names[m_fill])
            return ((int32_t)(pos - &m_names[0]));
        else
            return -1;
    }

private:
    FILE *m_streams[N ? N : 1];
    T m_names[N ? N : 1];
    size_t m_fill;
    bool m_DEBUG;
};

template <size_t N>
class WriteFileUsingCstdio<N, void>
{
public:
    WriteFileUsingCstdio()
    {
        m_DEBUG = false;
        m_fill = 0;
    }

    bool set_file(const string &filePath_, const string &mode = "ab")
    {
        FILE *fptr;
        int32_t index = 0;

        if ((fptr = fopen(filePath_.c_str(), mode.c_str())) == NULL)
        {
            cout << "WriteFileUsingCstdio: Cannot open file for write " << endl;
            throw string("WriteFileUsingCstdio: Cannot open file for write ") + filePath_;
        }
        else
        {
            cout << "registering file " << filePath_ << " for writes" << endl;
            m_streams[index] = fptr;
            index++;
        }

        m_fill = index;
        return true;
    }

    WriteStates stream_close()
    {
        int32_t fd;

        for (int32_t i = 0; i < m_fill; i++)
        {
            if ((fd = fileno(m_streams[i])) == -1)
            {
                cerr << "Error in retrieving fd for file : " << endl;
                cerr << "Consequence -> flush will not work, force closing, may lose some data"
                     << endl;
                fclose(m_streams[i]);
                continue;
            }

            fsync(fd);
            fclose(m_streams[i]);
        }

        return WRITE_SUCC;
    }

    /*!
     * @brief this method writes the data to the set file stream
     * @param bytes_ is the number of bytes to write
     * @param records_ is the number of records corresponding to the bytes
     */
    WriteStates stream_write_all(const void *data, size_t bytes_, size_t records_)
    {
        bool all_writes_success = true;
        int32_t index = 0;

        while (index <= m_fill - 1)
        {
            if (fwrite_unlocked(data, bytes_, records_, m_streams[index]) == 0)
            {
                all_writes_success = false;
            }

            index++;
        }

        if (all_writes_success)
        {
            return WRITE_SUCC;
        }
        else
        {
            return WRITE_ERR;
        }
    }

    int64_t stream_bytes()
    {
        int32_t index = 0;
        int64_t total_size = 0;

        while (index <= m_fill - 1)
        {
            int32_t fd = fileno(m_streams[index]);
            struct stat sb;

            if (fstat(fd, &sb) == -1)
            {
                throw string("ERROR: fstat failed");
            }

            total_size += sb.st_size;
            index++;
        }

        return total_size;
    }

private:
    FILE *m_streams[N ? N : 1];
    size_t m_fill;
    bool m_DEBUG;
};

#endif
