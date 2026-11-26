/*
 * SimpleAsyncTask.h
 *
 *  Created on: 24-Jul-2013
 */

#ifndef TASK_H_
#define TASK_H_

#ifdef DEBUGM
#define DEBUGPRINT(msg) std::cout << "Thread ID: " << std::this_thread::get_id() << " " << msg << std::endl;
#else
#define DEBUGPRINT(msg)
#endif

#include <thread>
#include <list>
#include "c11compatible.h"
namespace illuminati
{
    namespace infra
    {
        struct passItOn
        {
            template <typename... T>
            passItOn(T...)
            {
            }
        };

        template <class ValueType, class QueueType, class Reader, class Writer, class CallBack, int NumberOfReaders = 1, typename... Args>
        class Task
        {
        public:
            typedef std::list<std::thread> ThreadList;
            typedef std::list<Reader *> ReaderList;

            Task() : m_writer(nullptr), m_bulkRead(false), m_stopTask(false)
            {
                m_numberOfReaders = NumberOfReaders;

                if (m_numberOfReaders > 1)
                {
                    throw "Multi reader not supported";
                }
            }

            ~Task()
            {
            }

            void start(Args... args)
            {
                m_writer = m_q.createSingleWriter();

                DEBUGPRINT("starting task");
                for (int i = 0; i < m_numberOfReaders; ++i)
                {
                    Reader *reader = m_q.createReader();

                    m_readers.push_back(reader);
                    m_threads.push_back(std::thread(&Task::processTask, this, reader, m_writer, args...));
                }
            }

            void stop()
            {
                m_stopTask++;
                DEBUGPRINT("stopping");

                for (auto litem = m_threads.begin();
                     litem != m_threads.end(); ++litem)
                {
                    if ((*litem).joinable())
                    {
                        DEBUGPRINT("joinable");
                        (*litem).join();
                    }
                }

                DEBUGPRINT("all joined" << std::endl);

                for (auto aitem = m_readers.begin();
                     aitem != m_readers.end(); ++aitem)
                {
                    DEBUGPRINT("delete");
                    delete *aitem;
                }

                delete m_writer;
            }

            void post(ValueType &value)
            {
                DEBUGPRINT("posting");
                // queue add
                if (m_writer)
                {
                    while (m_writer->isFull())
                    {
                        DEBUGPRINT("Write full. sleeping...");
                        usleep(1);
                    }

                    m_writer->enqueue(value);
                }
                else
                {
                    //
                    throw "Task::start not called.";
                }
            }

            void setBulkProcess(bool value)
            {
                m_bulkRead = value;
            }

        protected:
            void processTask(Reader *reader, Writer *writer, Args &&...args)
            {
                // get arg
                CallBack func(args...);
                // CallBack<Args> func (args);
                DEBUGPRINT("running");

                while (true)
                {
                    if (m_stopTask.load())
                    {
                        DEBUGPRINT("Stop set");

                        if (m_bulkRead)
                        {
                            int size = reader->copyTillCurrentHead(func.getDestination());
                            func(size);
                        }
                        else
                        {
                            // if q is not empty read off till end.
                            while (!reader->isEmpty())
                            {
                                func(reader->dequeue());
                            }
                        }

                        break;
                    }
                    else
                    {
                        // if queue is not empty
                        if (!reader->isEmpty())
                        {
                            DEBUGPRINT("queue not empty");

                            if (m_bulkRead)
                            {
                                int size = reader->copyTillCurrentHead(func.getDestination());
                                func(size);
                            }
                            else
                            {
                                while (!reader->isEmpty())
                                {
                                    func(reader->dequeue());
                                }
                            }
                        }
                        else
                        {
                            DEBUGPRINT("no work. sleeping...");
                            usleep(1);
                        }
                    }
                }
            }

        private:
            // no copy, no assign
            Task(const Task &);
            Task &operator=(const Task &);

            // A queue is expected.
            QueueType m_q;
            // multiple readers on different threads,
            // to read off tasks from the queue and run them
            ReaderList m_readers;
            // single writer; as in 1 thread to post a task onto the
            //  queue.
            Writer *m_writer;

            // thread containers
            ThreadList m_threads;
            // std::thread m_taskWriterThread;

            bool m_bulkRead;

            std::atomic<int> m_stopTask;
            int m_numberOfReaders;

        }; /* class SimpleAsyncTask */
    } /* namespace infra */
} /* namespace illuminati */
#endif /* TASK_H_ */
