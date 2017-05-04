#ifndef VM_UTILS_THREAD_POOL_H
#define VM_UTILS_THREAD_POOL_H
#include <functional>
#include <memory>

namespace vm {

class Job {
    friend class ThreadPoolImpl;
    std::weak_ptr<struct JobEntry> job_entry;
    Job(std::weak_ptr<struct JobEntry> entry) : job_entry(entry) {}

public:
    Job() : job_entry() {}
};

class ThreadPool {
    std::unique_ptr<class ThreadPoolImpl> m_impl;

public:
    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;

    /** Creates a thread pool with @p num_threads workers available */
    ThreadPool(std::size_t num_threads);
    /** Destroys a thread pool waiting on the any remaining jobs to complete */
    ~ThreadPool();
    /** Enqueues a job to be done by the worker threads */
    Job enqueue(std::function<void()> &&job);
    /** Cancels a job if it is not yet being processed by the worker threads */
    void cancel(Job job);
    /** Terminates the thread pool - finishing all pending jobs */
    void terminate();
};

} // namespace vm

#endif
