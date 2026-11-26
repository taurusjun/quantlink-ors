#ifndef _CONC_HP_H
#define _CONC_HP_H
#include <utility>
#include "typeutils.h"
#include "util.h"
#include "rwlock.h"
#include "rwlock_guard.h"
#include "caslock.h"
#include <atomic>

#define CHP_JUMP_(key, num_probes) (num_probes)

namespace illuminati
{
    namespace ds
    {

        template <typename key_t, typename value_t, key_t empty_key, key_t deleted_key, typename hash_func = std::hash<key_t>, typename eq_func = std::equal_to<key_t>>
        class concurrent_hash_pool // TODO : better name
        {
        protected:
            typedef typename TypeWithSize<sizeof(key_t)>::UInt Bits;

        public:
            struct keyval_t
            {
                std::atomic<key_t> key;
                value_t *val;
                keyval_t() : key(empty_key) { val = new value_t(); }
            };

        protected:
            RWLock resize_lock;
            keyval_t *table;
            CasLock *locks;
            size_t bucket_count;

            hash_func hash;
            eq_func key_equals;

            keyval_t *_find(const key_t key, size_t startbucket)
            {
                size_t num_probes = 0; // how many times we've probed
                const size_t bucket_count_minus_one = bucket_count - 1;
                size_t bucknum = startbucket;

                while (true)
                { // probe until something happens
                    auto tkey = table[bucknum].key.load(std::memory_order_relaxed);
                    if (test_empty(tkey))
                    { // bucket is empty
                        return NULL;
                    }
                    else if (test_deleted(key))
                    {
                        // do nothing.. continue forward
                    }
                    else if (key_equals(key, tkey))
                    {
                        return &(table[bucknum]);
                    }
                    ++num_probes; // we're doing another probe
                    bucknum = (bucknum + CHP_JUMP_(key, num_probes)) & bucket_count_minus_one;
                    if (num_probes > bucket_count)
                        return NULL;
                }
            }

            bool cas_key(size_t bucknum, const key_t k_new, key_t k_old)
            {
                key_t k_replace = k_new;
                while (true)
                {
                    // try to atomically swap old key with new
                    if (table[bucknum].key.compare_exchange_weak(k_replace, k_new, std::memory_order_seq_cst, std::memory_order_relaxed))
                    {
                        // succeeded, we are done
                        return true;
                    }
                    // didn't succeed. May need to retry, or someone else got here before us.
                    if (k_replace != k_old)
                    {
                        // yup, someone else got here before us
                        k_old = k_replace;
                        return false;
                    }
                    // retry
                }
            }

            // unsafe insert, should only be called while holding a write lock on resize_lock
            void _insert(keyval_t *newtable, size_t newsize, const key_t key, value_t *val)
            {
                const size_t bucket_count_minus_one = newsize - 1;
                size_t bucknum = hash(key) & bucket_count_minus_one;
                size_t num_probes = 0;

                while (num_probes <= newsize)
                {
                    auto tkey = newtable[bucknum].key.load(std::memory_order_relaxed);
                    if (test_empty(tkey))
                    {
                        newtable[bucknum].key.store(key, std::memory_order_relaxed);
                        newtable[bucknum].val = val;
                        return;
                    }
                    else if (test_deleted(tkey))
                    {
                        // Should not happen in this function
                        throw "error";
                        return;
                    }
                    else if (key_equals(tkey, key))
                    {
                        // Should not happen in this function
                        throw "error";
                    }
                    // otherwise its a filled slot. reprobe.

                    ++num_probes; // we're doing another probe
                    bucknum = (bucknum + CHP_JUMP_(key, num_probes)) & bucket_count_minus_one;
                }

                // This is a problem. The table should already be large enough to allow this insertion.
                throw "error";
            }

            void resize(size_t target_size)
            {
                ds::wlock_guard<RWLock> wl(resize_lock);
                if (bucket_count >= target_size) // someone finished the resize before us
                    return;

                auto newtable = new keyval_t[target_size];
                auto newlocks = new CasLock[target_size];
                for (size_t i = 0; i < bucket_count; i++)
                {
                    if (test_empty(table[i].key) || test_deleted(table[i].key))
                        continue;
                    _insert(newtable, target_size, table[i].key, table[i].val);
                }
                table = newtable;
                locks = newlocks;
                bucket_count = target_size;
            }

            bool test_empty(const key_t &key)
            {
                // compare bitwise, do not use == operator
                return (reinterpret_cast<Bits>(key) == reinterpret_cast<Bits>(empty_key));
            }

            bool test_deleted(const key_t &key)
            {
                // compare bitwise, do not use == operator
                return (reinterpret_cast<Bits>(key) == reinterpret_cast<Bits>(deleted_key));
            }

        public:
            typedef keyval_t *iterator;

            concurrent_hash_pool(size_t size)
            {
                bucket_count = getNextHigherPow2(size);
                if (bucket_count < 512)
                    bucket_count = 512;
                table = new keyval_t[bucket_count];
                locks = new CasLock[bucket_count];
            }

            keyval_t *acquire_new(key_t k)
            {
                size_t current_bucket_count = 1;
                while (true)
                {
                    resize_lock.rlock();

                    size_t num_probes = 0; // how many times we've probed
                    current_bucket_count = bucket_count;
                    const size_t bucket_count_minus_one = current_bucket_count - 1;
                    size_t bucknum = hash(k) & bucket_count_minus_one;

                    // nothing that hashes to this bucket can be inserted.
                    locks[bucknum].lock();

                    while (num_probes <= current_bucket_count)
                    {
                        auto tkey = table[bucknum].key.load(std::memory_order_relaxed);
                        if (test_empty(tkey))
                        {
                            if (cas_key(bucknum, k, empty_key))
                            {
                                // key was empty, and we succeeded in atomicaly putting our own key there.
                                return &(table[bucknum]);
                            }
                            // else key is no longer the empty key. Someone else is doing an insert. Move forward.
                        }
                        else if (test_deleted(tkey))
                        {
                            if (cas_key(bucknum, k, deleted_key))
                            {
                                // key was deleted, and we succeeded in atomically putting our own key there.
                                return &(table[bucknum]);
                            }
                        }
                        else if (key_equals(tkey, k))
                        {
                            // value already exists. Return false
                            return &(table[bucknum]);
                        }
                        // otherwise its a filled slot. reprobe.

                        ++num_probes; // we're doing another probe
                        bucknum = (bucknum + CHP_JUMP_(k, num_probes)) & bucket_count_minus_one;
                    }
                    // need to resize;

                    locks[bucknum].unlock();
                    resize_lock.runlock();
                    resize(current_bucket_count * 2);
                } // retry insert now on bigger table
            }

            keyval_t *acquire(const key_t key)
            {
                // prevent resizes while something has been acquired
                resize_lock.rlock();
                const size_t bucket_count_minus_one = bucket_count - 1;
                size_t bucknum = hash(key) & bucket_count_minus_one;

                // This will prevent insertions with the same hash value, but that is not the point
                // The point is: prevent two threads from 'acquire'ing the same key
                locks[bucknum].lock();

                keyval_t *kv = _find(key, bucknum);

                if (kv == NULL)
                {
                    locks[bucknum].unlock();
                    resize_lock.runlock();
                }

                return kv;
            }

            void release(keyval_t *kv)
            {
                const size_t bucket_count_minus_one = bucket_count - 1;
                size_t bucknum = hash(kv->key) & bucket_count_minus_one;
                locks[bucknum].unlock();
                resize_lock.runlock();
            }

            void erase_release(keyval_t *kv)
            { // erase and release
                if (kv == NULL)
                    return;
                new (kv->val) value_t();
                kv->key.store(deleted_key, std::memory_order_relaxed);
                locks[hash(kv->key) & (bucket_count - 1)].unlock();
                resize_lock.runlock();
            }
        };
    } // namespace ds
} // namespace illuminati
#endif
