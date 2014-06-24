#include "shared_state_traits_backtrace.h"
#include "barrier.h"
#include "exceptionassert.h"
#include "trace_perf.h"
#include "demangle.h"
#include "tasktimer.h"

#include <future>

using namespace std;

std::function<void(double, double, void*, const std::type_info&)> shared_state_traits_backtrace::default_warning =
        [](double T, double V, void*, const std::type_info& i)
        {
            auto bt = Backtrace::make ();
            std::string tn = demangle(i);

            std::async(std::launch::async, [T, V, tn, bt]{
                TaskInfo(boost::format("!!! Warning: Lock of %s was held for %s > %s. %s") %
                         tn % TaskTimer::timeToString (T) % TaskTimer::timeToString (V) % bt.value ().to_string ());
            });
        };


namespace shared_state_traits_backtrace_test {

class A {
public:
    struct shared_state_traits: shared_state_traits_backtrace {
        double timeout() override { return 0.002; }
        double verify_lock_time() override { return lock_time; }
        double lock_time = 0.001;
    };
};

} // namespace shared_state_traits_backtrace_test

using namespace shared_state_traits_backtrace_test;

void shared_state_traits_backtrace::
        test()
{
#ifndef SHARED_STATE_NO_TIMEOUT
    // shared_state can be extended with type traits to get, for instance,
    //  - backtraces on deadlocks from all participating threads,
    {
        typedef shared_state<A> ptr;
        ptr a{new A};
        ptr b{new A};
        a.traits ()->lock_time = 1;
        b.traits ()->lock_time = 1;

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

    // shared_state can be extended with type traits to get, for instance,
    //  - warnings on locks that are held too long.
    {
        shared_state<A> a{new A};

        std::string report_type;
        a.traits ()->exceeded_lock_time = [&report_type](float,float,void*,const std::type_info& i){ report_type = demangle (i); };

        auto w = a.write ();

        // Wait to make VerifyExecutionTime detect that the lock was kept too long
        this_thread::sleep_for (chrono::milliseconds{10});

        EXCEPTION_ASSERT(report_type.empty ());
        w.unlock ();
        EXCEPTION_ASSERT_EQUALS(report_type, "shared_state_traits_backtrace_test::A");

        {
            int N = 10000;

            TRACE_PERF("warnings on locks that are held too long should cause a low overhead");
            for (int i=0; i<N; i++)
            {
                a.write ();
                a.read ();
            }
        }
    }
}
