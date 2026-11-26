#ifndef _MmapArray_H
#define _MmapArray_H

#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/stat.h>
#include <iostream>
#include <string>
#include <string.h>
#include <fcntl.h>

namespace illuminati
{
    namespace infra
    {
        template <typename T>
        class MmapArray
        {
        protected:
            T *m_data;
            int m_fd;
            size_t m_rawSize;
            size_t m_size;
            bool m_initialized;

        public:
            MmapArray() : m_initialized(false) {}

            MmapArray(MmapArray &m) : m_data(m.m_data), m_fd(m.m_fd), m_rawSize(m.m_size), m_size(m.m_size), m_initialized(m.m_initialized)
            {
            }

            bool init(const char *filename, bool readOnly = true)
            {
                int fileFlags;
                int mappingProtection;
                int mappingFlags;
                if (readOnly)
                {
                    fileFlags = O_RDONLY;
                    mappingProtection = PROT_READ;
                    mappingFlags = MAP_SHARED;
                }
                else
                {
                    fileFlags = O_RDWR;
                    mappingProtection = PROT_WRITE | PROT_READ;
                    mappingFlags = MAP_SHARED;
                }
                m_fd = open(filename, fileFlags);
                struct stat fdstat;
                if (fstat(m_fd, &fdstat) == -1)
                    return false;

                m_rawSize = fdstat.st_size;
                m_size = m_rawSize / sizeof(T);

                m_data = (T *)mmap64(NULL, m_rawSize, mappingProtection, mappingFlags, m_fd, 0);
                if (m_data == MAP_FAILED)
                    return false;
                madvise(m_data, m_rawSize, POSIX_MADV_WILLNEED | POSIX_MADV_SEQUENTIAL);
                m_initialized = true;
                return true;
            }

            inline __attribute__((always_inline))
            T
            operator[](size_t offset)
            {
                return m_data[offset];
            }

            size_t size()
            {
                return m_size;
            }

            ~MmapArray()
            {
                if (m_initialized)
                {
                    munmap(m_data, m_rawSize);
                    close(m_fd);
                }
            }
        };
    } // namespace infra
}

#endif
