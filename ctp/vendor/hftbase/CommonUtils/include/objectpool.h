#ifndef OBJECTPOOL_H_
#define OBJECTPOOL_H_
#include <queue>
#include <functional>

namespace illuminati
{
    namespace ds
    {
        template <typename T>
        class ObjectPool
        {
        protected:
            struct Chunk
            {
                T data;
                Chunk *next;
            };

            Chunk *head;
            unsigned int numfree;
            std::queue<Chunk *> pools;

            void create_new_pool(unsigned int, std::function<void(T *)>);

        public:
            ObjectPool() : numfree(0), head(0){};
            ~ObjectPool();

            void init(unsigned int num);
            void init(unsigned int, std::function<void(T *)> &);
            void extend(unsigned int num);
            void extend(unsigned int, std::function<void(T *)> &);

            T *allocate();
            void deallocate(T *);

            unsigned int available();
        };
    }
}
#endif
