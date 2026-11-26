#ifndef _READFILEUSINGMMAP_H
#define _READFILEUSINGMMAP_H

#include <string>
#include <cstring>
#include <iostream>
#include <cstddef>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unordered_set>
#include <cassert>

#include "ReadStates.h"
#include "itimer.h"
#include "marketdelta.h"
#include "stringutils.h"
#include "configreader.h"

const int32_t CACHE_SIZE = 4 * 1024;

using namespace std;

class ReadFileUsingMMAP
{
public:
    ReadFileUsingMMAP() : m_page_fault(true)
    {
    }

    ReadFileUsingMMAP(illuminati::Configfile &cfg)
    {
        string filepath = cfg.getString_s("FileBasedStreamReader", "FILE_PATH");
        set_stream(filepath);
    }

    bool init(illuminati::Configfile &cfg, std::string section)
    {
        string filepath = cfg.getString_s(section.c_str(), "FILE_PATH");
        set_stream(filepath);
        return true;
    }

    const int getSocket(void) const
    {
        return 0;
    }

    void set_stream(const string &filePath_, bool adviseFlag_ = false, int32_t adviseOp_ = 0)
    {
        m_fd = open(filePath_.c_str(), O_RDONLY);
        if (m_fd == -1)
        {
            throw string("ERROR: unable to open file : " + filePath_);
        }

        if (fstat(m_fd, &m_sb) == -1)
        {
            throw string("ERROR: fstat failed");
        }

        if (!S_ISREG(m_sb.st_mode))
        {
            throw string("ERROR: the input file is not a regular file");
        }

        m_beginAddr = (char *)mmap(0, m_sb.st_size, PROT_READ, MAP_SHARED, m_fd, 0);
        if (m_beginAddr == MAP_FAILED)
        {
            throw string("ERROR: Failure to create mmap");
        }

        m_streamiterator = m_beginAddr;
        m_offset = 0;

        if (close(m_fd) == -1)
        {
            throw string("ERROR: failure in file close");
        }
        cout << "Successfully MMAP'ed file " << filePath_ << endl;

        if (adviseFlag_)
        {
            switch (adviseOp_)
            {
            case 0:
                madvise(m_beginAddr, m_sb.st_size, MADV_NORMAL);
                break;
            case 1:
                madvise(m_beginAddr, m_sb.st_size, MADV_RANDOM);
                break;
            case 2:
                madvise(m_beginAddr, m_sb.st_size, MADV_SEQUENTIAL);
                break;
            case 3:
                madvise(m_beginAddr, m_sb.st_size, MADV_DONTNEED);
                break;
            case 4:
                madvise(m_beginAddr, m_sb.st_size, MADV_WILLNEED);
                break;
            default:
                break;
            }
        }
    }

    bool ReceiveMessage(unsigned char *buf, size_t chunk, size_t &bytes_read, std::string &error, uint64_t ts = 0)
    {
        ReadStates ret = stream_read((char *)buf, chunk, bytes_read);
        switch (ret)
        {
        case READ_SUCC:
            return true;
        case FILE_EOF:
            return false;
        default:
        {
            break;
        }
        }
    }

    ReadStates stream_read(char *buf, size_t chunk, size_t &bytes_read)
    {
        if (chunk < (m_sb.st_size - m_offset))
        {
            bytes_read = chunk;
        }
        else
        {
            bytes_read = (m_sb.st_size - m_offset);
        }

        if (bytes_read > 0)
        {
            memcpy(buf, m_streamiterator, bytes_read);
            // 	    *addr = m_streamiterator;
            m_streamiterator += bytes_read;
            m_offset += bytes_read;
            return READ_SUCC;
        }
        else
        {
            return FILE_EOF;
        }
    }

    void set_filter(const string &filter_)
    {
        m_filter = filter_;
    }

    ReadStates stream_read_filtered(char *buf, int32_t chunk, size_t &bytes_read)
    {
        using illuminati::infra::MarketUpdate;

        if (chunk < (m_sb.st_size - m_offset))
        {
            bytes_read = chunk;
        }
        else
        {
            bytes_read = (m_sb.st_size - m_offset);
        }

        if (bytes_read > 0)
        {
            memcpy(buf, m_streamiterator, bytes_read);
            m_streamiterator += bytes_read;
            m_offset += bytes_read;

            MarketUpdate *up = (MarketUpdate *)buf;

            if (strcmp(illuminati::trim(string(up->m_symbol)).c_str(), m_filter.c_str()) == 0)
            {
                return READ_SUCC;
            }
            else
            {
                memset(buf, '\0', bytes_read);
                stream_read_filtered(buf, chunk, bytes_read);
            }
            // 	    *addr = m_streamiterator;
            // 	    return READ_SUCC;
        }
        else
        {
            return FILE_EOF;
        }
    }

    ReadStates stream_read_at(char *buf, off_t offset, size_t chunk)
    {
        // 	m_p.start();
        if (offset < m_sb.st_size)
        {
            char *stream_iterator = m_beginAddr + offset;
            memcpy(buf, stream_iterator, chunk);
            return READ_SUCC;
        }
        else
        {
            cerr << "MMAP: Size=" << chunk << " at Offset=" << offset << " asked is not available, Returning.. EOF" << endl;
            return FILE_EOF;
        }
        // 	m_p.stop();
        // 	cout<<"time to taken to get packet in mmap "<<m_p.getelapsedMicroSecs()<<endl;
    }

    ReadStates stream_cacheread_at(char *buf, off_t offset, size_t chunk)
    {
        if (offset > m_sb.st_size)
        {
            cerr << "MMAP: Offset asked is not available, Returning.. EOF" << endl;
            return FILE_EOF;
        }

        if (m_page_fault)
        {
            // get the current data asked for
            memcpy(buf, m_beginAddr + offset, chunk);
            // build cache
            off_t local_offset = offset + chunk;
            if (local_offset < m_sb.st_size)
            {
                int32_t bytes_copy = 0;
                if (CACHE_SIZE < (m_sb.st_size - local_offset))
                {
                    bytes_copy = CACHE_SIZE;
                }
                else
                {
                    bytes_copy = (m_sb.st_size - local_offset);
                }
                memcpy(m_cache, (m_beginAddr + offset + chunk), bytes_copy);
                m_cache_start = offset + chunk;
                m_cache_end = offset + chunk + bytes_copy;
                m_page_fault = false;
            }
        }
        else
        {
            if ((offset >= m_cache_start) && (m_cache_end - chunk) >= offset)
            {
                char *cache_iterator = m_cache + (offset - m_cache_start);
                memcpy(buf, cache_iterator, chunk);
            }
            else
            {
                m_page_fault = true;
                stream_cacheread_at(buf, offset, chunk);
            }
        }

        return READ_SUCC;
    }

    void set_reverse()
    {
        m_offset = m_sb.st_size - illuminati::infra::HeaderSize;
        m_streamiterator = m_beginAddr + m_offset;
    }

    ReadStates stream_read_reverse(char *buf, int32_t chunk, size_t &bytes_read)
    {
        if (chunk < m_offset)
        {
            bytes_read = chunk;
        }
        else
        {
            if (m_offset != 0)
                bytes_read = m_offset;
            else
                bytes_read = 0;
        }

        if (bytes_read > 0)
        {
            memcpy(buf, m_streamiterator, bytes_read);
            // 	    *addr = m_streamiterator;
            m_streamiterator -= bytes_read;
            m_offset -= bytes_read;
            return READ_SUCC;
        }
        else
        {
            return FILE_EOF;
        }
    }

    off_t stream_size()
    {
        return m_sb.st_size;
    }
    void stream_close()
    {
        if (munmap(m_beginAddr, m_sb.st_size) == -1)
        {
            throw string("ERROR: failure in unmapping file");
        }
    }

private:
    string m_filePath;
    struct stat m_sb;
    char *m_beginAddr;
    int32_t m_fd;
    char *m_streamiterator;
    int64_t m_cache_start;
    int64_t m_cache_end;
    off_t m_offset;
    char m_cache[CACHE_SIZE];
    bool m_page_fault;
    string m_filter;
    illuminati::ITime_HighResolution m_p;
};

#endif
