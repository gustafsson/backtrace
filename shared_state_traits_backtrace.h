#ifndef SHARED_STATE_BACKTRACE_H
#define SHARED_STATE_BACKTRACE_H

#include "shared_state.h"
#include "backtrace.h"

#include <thread>
#include <boost/exception/all.hpp>

template<class T>
class lock_failed_boost
        : public shared_state<T>::lock_failed
        , public virtual boost::exception
{};


/**
 * @brief The shared_state_traits_backtrace should provide backtraces on
 * lock_failed exceptions.
 *
 * class MyType {
 * public:
 *     struct shared_state_traits: shared_state_traits_backtrace {
 *         double timeout() { return 0.002; }
 *     };
 * ...
 * };
 */
struct shared_state_traits_backtrace: shared_state_traits_default {
    virtual double timeout () { return shared_state_traits_default::timeout (); }

    template<class T>
    void timeout_failed () {
        /*
        When a timeout occurs on a lock, this makes an attempt to detect
        deadlocks. The thread with the timeout is blocked long enough (same
        timeout as in the failed lock attempt) for any other thread that is
        deadlocking with this thread to also fail its lock attempt.
        */
        std::this_thread::sleep_for (std::chrono::duration<double>{timeout()});

        BOOST_THROW_EXCEPTION(lock_failed_boost<T>()
                              << Backtrace::make ());
    }

    static void test();
};

#endif // SHARED_STATE_BACKTRACE_H
