#ifndef BARRIER_H
#define BARRIER_H

#include <atomic>
#include <mutex>
#include <thread>

/**
 * @brief The spinning_barrier class should provide a lock-free spinning
 * barrier.
 *
 * Based on chill @ http://stackoverflow.com/a/8120707
 */
class spinning_barrier
{
public:
    /**
     * @brief spinning_barrier
     * @param n number of threads to participate in the barrier. If 'n' is
     *        larger than the number of cores, consider using locking_barrier
     *        instead.
     */
    spinning_barrier (unsigned int n) : n_ (n), nwait_ (0), step_(0), yield_(n > std::thread::hardware_concurrency()) {}
    spinning_barrier (unsigned int n, bool yield) : n_ (n), nwait_ (0), step_(0), yield_(yield) {}

    bool wait ()
    {
        unsigned int step = step_.load ();

        if (nwait_.fetch_add (1) == n_ - 1)
        {
            // OK, last thread to come.
            nwait_.store (0);
            step_.fetch_add (1);
            return true;
        }
        else if (yield_)
        {
            while (step_.load () == step)
                std::this_thread::yield();
            return false;
        }
        else
        {
            while (step_.load () == step)
                ;
            return false;
        }
    }

private:
    // Number of synchronized threads.
    const unsigned int n_;

    // Number of threads currently spinning.
    std::atomic<unsigned int> nwait_;

    // Number of barrier syncronizations completed so far, it's OK to wrap.
    std::atomic<unsigned int> step_;

    // Whether to yield or not
    bool yield_;

public:
    static void test();
};


/**
 * @brief The locking_barrier class should provide a non-spinning barrier.
 */
class locking_barrier
{
public:
    /**
     * @brief locking_barrier
     * @param n number of threads to participate in the barrier. If 'n' is
     *        smaller than or equal to the number of cores, consider using
     *        locking_barrier instead.
     */
    locking_barrier (unsigned int n) : n_ (n), nwait_ (0), step_(0) {}

    bool wait ()
    {
        std::unique_lock<std::mutex> l(m_);
        unsigned int step = step_;

        if (nwait_++ == n_ - 1)
        {
            // OK, last thread to come.
            nwait_ = 0;
            step_++;
            cv_.notify_all ();
            return true;
        }
        else
        {
            while (step_ == step)
                cv_.wait (l);
            return false;
        }
    }

private:
    // Number of synchronized threads.
    const unsigned int n_;

    // Number of threads currently spinning.
    unsigned int nwait_;

    // Number of barrier syncronizations completed so far, it's OK to wrap.
    unsigned int step_;

    std::mutex m_;
    std::condition_variable cv_;

public:
    static void test();
};

#endif // BARRIER_H
