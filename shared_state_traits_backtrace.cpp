#include "shared_state_traits_backtrace.h"
#include "barrier.h"
#include "exceptionassert.h"

#include <future>

using namespace std;

class with_timeout_2_with_boost_exception {
public:
    struct shared_state_traits: shared_state_traits_backtrace {
        double timeout() { return 0.002; }
    };
};


void shared_state_traits_backtrace::
        test()
{
    // It should provide backtraces on lock_failed exceptions.

#ifndef SHARED_STATE_NO_TIMEOUT
    // It should be extensible enough to let clients efficiently add features like
    //  - backtraces on failed locks
    {
        typedef shared_state<with_timeout_2_with_boost_exception> ptr;
        ptr a{new with_timeout_2_with_boost_exception};
        ptr b{new with_timeout_2_with_boost_exception};

        spinning_barrier barrier(2);

        std::function<void(ptr,ptr)> m = [&barrier](ptr p1, ptr p2) {
            try {
                auto w1 = p1.write ();
                barrier.wait ();
                auto w2 = p2.write ();

                // never reached
                EXCEPTION_ASSERT(false);
            } catch (lock_failed& x) {
                // cheeck that a backtrace was embedded into the lock_failed exception
                const Backtrace* backtrace = boost::get_error_info<Backtrace::info>(x);
                EXCEPTION_ASSERT(backtrace);
            }
        };

        // Lock a and b in opposite order in f1 and f2
        future<void> f1 = async(launch::async, [&](){ m (b, a); });
        future<void> f2 = async(launch::async, [&](){ m (a, b); });

        f1.get ();
        f2.get ();
    }
#endif
}
