#ifndef SHARED_STATE_TRAITS_BACKTRACE_H
#define SHARED_STATE_TRAITS_BACKTRACE_H

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
 * @brief The shared_state_traits_backtrace struct should provide backtraces on
 * lock_failed exceptions. It should issue a warning if the lock is kept too long.
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
    virtual double verify_lock_time() { return timeout()/2.0f; }

    template<class T>
    void timeout_failed (T*) {
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

    template<class T>
    void locked(T*) {
        start = std::chrono::high_resolution_clock::now ();
    }

    template<class T>
    void unlocked(T* t) noexcept {
        std::chrono::duration<double> diff = std::chrono::high_resolution_clock::now () - start;
        double D = diff.count ();
        double V = verify_lock_time ();
        if (D > V && exceeded_lock_time)
            exceeded_lock_time (D, V, (void*)t, typeid(*t));
    }

    static std::function<void(double, double, void*, const std::type_info&)> default_warning;
    std::function<void(double, double, void*, const std::type_info&)> exceeded_lock_time = default_warning;

    static void test();

private:
    std::chrono::high_resolution_clock::time_point start;
};

#endif // SHARED_STATE_TRAITS_BACKTRACE_H
