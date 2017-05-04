#include "thread-pool.h"
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <vector>
#include <list>
#include <functional>
#include <iostream>
#include <condition_variable>

#include "utils/log.h"

using namespace std;
namespace vm {

using JobPtr = shared_ptr<JobEntry>;
using JobWeakPtr = weak_ptr<JobEntry>;

struct JobEntry {
    function<void()> job;
    list<JobPtr>::iterator self;

    JobEntry(function<void()> &&job) : job(move(job)), self() {}
};

class ThreadPoolImpl {
    mutex m_mutex;
    atomic<bool> m_running;
    list<thread> m_workers;
    condition_variable m_jobs_cv;
    list<JobPtr> m_queue;

public:
    ThreadPoolImpl(size_t num_threads)
            : m_mutex(), m_running(true), m_workers(), m_queue() {
        for (size_t i = 0; i < num_threads; ++i) {
            m_workers.emplace_back([&](){
                while (m_running.load()) {
                    function<void()> job;
                    {
                        unique_lock<mutex> queue_lock(m_mutex);
                        if (!m_queue.size()) {
                            m_jobs_cv.wait(queue_lock, [&]() {
                                return m_queue.size() || !m_running.load();
                            });
                            // This must have been the second condition - i.e.
                            // m_running == false.
                            if (!m_queue.size()) {
                                return;
                            }
                        }
                        job = m_queue.front()->job;
                        m_queue.pop_front();
                    }
                    m_jobs_cv.notify_all();
                    try {
                        job();
                    } catch (exception &e) {
                        LOG(warning) << "Exception thrown by the job: " << e.what();
                    }
                }
            });
        }
        LOG(info) << "Spawned " << m_workers.size() << " worker threads";
    }

    ~ThreadPoolImpl() {
        terminate();
    }

    Job enqueue(function<void()> &&job) {
        if (!m_running.load()) {
            throw logic_error("Cannot enqueue job on a terminated thread-pool");
        }
        auto entry = make_shared<JobEntry>(move(job));
        {
            lock_guard<mutex> queue_lock(m_mutex);
            m_queue.push_back(entry);
            entry->self = --m_queue.end();
        }
        m_jobs_cv.notify_all();
        return Job{ entry };
    }

    void cancel(Job job) {
        if (!m_running.load()) {
            throw logic_error("Cannot cancel job on a terminated thread-pool");
        }
        lock_guard<mutex> queue_lock(m_mutex);

        if (auto entry = job.job_entry.lock()) {
            m_queue.erase(entry->self);
        }
    }

    void terminate() {
        if (!m_running.load()) {
            return;
        }

        while (true) {
            {
                lock_guard<mutex> queue_lock(m_mutex);
                if (!m_queue.size()) {
                    break;
                }
            }
            m_jobs_cv.notify_all();
            using namespace std::literals::chrono_literals;
        }
        m_running.store(false);
        m_jobs_cv.notify_all();
        for (thread &t : m_workers) {
            t.join();
        }
    }
};

ThreadPool::ThreadPool(size_t num_threads)
        : m_impl(make_unique<ThreadPoolImpl>(num_threads)) {}

ThreadPool::~ThreadPool() {
}

Job ThreadPool::enqueue(std::function<void()> &&job) {
    return m_impl->enqueue(move(job));
}

void ThreadPool::cancel(Job job) {
    m_impl->cancel(job);
}

void ThreadPool::terminate() {
    m_impl->terminate();
}

} // namespace vm
