#ifndef _CONCMAP_H_
#define _CONCMAP_H_
#include <unordered_map>
// #include <mutex>
// #include "caslock.h"
#include "rwlock.h"

namespace illuminati
{
    namespace ds
    {
        template <typename key_t, typename value_t,
                  unsigned int numbuckets = 16, typename Hash = std::hash<key_t>>
        class concurrent_unordered_map
        {
        protected:
            struct maplock
            {
                std::unordered_map<key_t, value_t *> map;
                // CasLock lock;
                RWLock lock;
            };

            maplock buckets[numbuckets];

            Hash hsh;
            static const unsigned int bucketshift = 16;

        public:
            std::pair<value_t *, bool> insert(std::pair<key_t, value_t *> kv)
            {
                size_t h = hsh(kv.first);
                unsigned int bucket_index = (h >> bucketshift) & (numbuckets - 1);

                std::pair<value_t *, bool> result;

                // std::lock_guard<CasLock> l(buckets[bucket_index].lock);

                buckets[bucket_index].lock.wlock();

                auto ins = buckets[bucket_index].map.insert(kv);
                result.second = ins.second;
                if (ins.second == false)
                {
                    result.first = ins.first->second;
                }

                buckets[bucket_index].lock.wunlock();
                return result;
            }

            value_t *find(const key_t &k)
            {
                size_t h = hsh(k);
                unsigned int bucket_index = (h >> bucketshift) & (numbuckets - 1);

                // std::lock_guard<CasLock> l(buckets[bucket_index].lock);

                buckets[bucket_index].lock.rlock();

                auto it = buckets[bucket_index].map.find(k);
                if (it == buckets[bucket_index].map.end())
                {
                    buckets[bucket_index].lock.runlock();
                    return NULL;
                }
                else
                {
                    buckets[bucket_index].lock.runlock();
                    return it->second;
                }
            }

            bool erase(const key_t &k)
            {
                size_t h = hsh(k);
                unsigned int bucket_index = (h >> bucketshift) & (numbuckets - 1);

                // std::lock_guard<CasLock> l(buckets[bucket_index].lock);
                buckets[bucket_index].lock.wlock();

                if (buckets[bucket_index].erase(k) == 0)
                {
                    buckets[bucket_index].lock.wunlock();
                    return false;
                }
                else
                {
                    buckets[bucket_index].lock.wunlock();
                    return true;
                }
            }
        };
    } // namespace ds
} // namespace illuminati
#endif
