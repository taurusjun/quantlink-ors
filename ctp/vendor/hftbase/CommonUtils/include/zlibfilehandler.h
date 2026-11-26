#ifndef __ZLIB_FILE_Reader_H
#define __ZLIB_FILE_Reader_H
#include "zlib.h"

namespace illuminati
{
    namespace io
    {
        class ZlibFileHandler
        {
        private:
            gzFile f;

        public:
            ZlibFileHandler()
            {
            }

            void open(const char *file, const char *mode = "r")
            {
                f = gzopen(file, mode);
            }

            bool good()
            {
                return (f != Z_NULL);
            }

            int read(void *buf, size_t len)
            {
                return gzread(f, buf, len);
            }

            void close()
            {
                gzclose(f);
            }
        };
    }
}

#endif
