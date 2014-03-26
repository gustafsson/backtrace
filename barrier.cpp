#include "barrier.h"
#include "exceptionassert.h"
#include "timer.h"

#include <thread>
#include <future>

using namespace std;

template<class barrier>
void simple_barrier_test() {
    bool a = true;
    barrier b{2};

    future<void> f = async(launch::async, [&a,&b]()
    {
        b.wait ();
        a = false;
        b.wait ();
    });

    this_thread::sleep_for (chrono::microseconds{10});

    EXCEPTION_ASSERT(a);
    b.wait ();
    b.wait ();
    EXCEPTION_ASSERT(!a);
    f.get ();
}


static pair<float, float> evalate(int N) {
    vector<future<void>> f(N);
    spinning_barrier sb (N+1);
    locking_barrier lb (N+1);

    int M = 20;

    Timer t(false);

    for (int i=0; i<f.size (); i++)
        f[i] = async(launch::async, [&]()
        {
            sb.wait ();
            for (int i=0; i<M; i++)
                sb.wait ();
        });

    sb.wait ();
    t.restart ();
    for (int i=0; i<M; i++)
        sb.wait ();
    double se = t.elapsed ();

    for (int i=0; i<f.size (); i++)
        f[i].get();


    for (int i=0; i<f.size (); i++)
        f[i] = async(launch::async, [&]()
        {
            lb.wait ();
            for (int i=0; i<M; i++)
                lb.wait ();
        });

    lb.wait ();
    t.restart ();
    for (int i=0; i<M; i++)
        lb.wait ();

    double le = t.elapsed ();

    for (int i=0; i<f.size (); i++)
        f[i].get();

    // printf("N = %d, spinning = %g, locking = %g\n", N, se, le);

    return pair<float,float>(se, le);
}


void spinning_barrier::
        test ()
{
    // It should provide a lock-free spinning barrier efficient when the number
    // of threads are fewer than the number of available cores.
    {
        simple_barrier_test<spinning_barrier>();
    }
}


void locking_barrier::
        test ()
{
    // It should behave like spinning_barrier but use a lock instead of spinning
    {
        simple_barrier_test<locking_barrier>();
    }

    {
        // A spinning lock is always fast if the barriers are reached simultaneous
        unsigned concurentThreadsSupported = std::max(1u, std::thread::hardware_concurrency());
        pair<float,float> e = evalate(10*concurentThreadsSupported);

        EXCEPTION_ASSERT_LESS (3*e.first, e.second);

        e = evalate((concurentThreadsSupported+1)/2);
        EXCEPTION_ASSERT_LESS (100*e.first, e.second);
    }
}
