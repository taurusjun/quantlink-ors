#ifndef __Common_Types_H
#define __Common_Types_H

typedef CircularQ(illuminati::md::MarketUpdateNew, illuminati::storagepolicy::SHMStorage,
                  INT(MICEX_MD_SHM_SIZE), INT(MICEX_MD_SHM_KEY), INT(IPC_CREAT | 0666)) MdShmQueueWriter;
typedef CircularQ(illuminati::md::MarketUpdateNew, illuminati::storagepolicy::SHMStorage,
                  INT(MICEX_MD_SHM_SIZE), INT(MICEX_MD_SHM_KEY), INT(0666)) MdShmQueueReader;

#endif
