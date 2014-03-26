/**
  This file only contains unit tests for spinning_barrier and locking_barrier.
  This file is not required for using either.
  */

#include "barrier.h"
#include "exceptionassert.h"
#include "timer.h"
#include "trace_perf.h"

#include <thread>
#include <future>

#include <boost/format.hpp>

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


static void evalate(int N) {
    vector<future<void>> f(N);
    spinning_barrier sb (N+1);
    locking_barrier lb (N+1);

    int M = 20;

    for (int i=0; i<f.size (); i++)
        f[i] = async(launch::async, [&]()
        {
            sb.wait ();
            for (int i=0; i<M; i++)
                sb.wait ();
        });

    {
        sb.wait ();
        TRACE_PERF(str(boost::format("spinning_barrier %d threads, %d times") % N % M));

        for (int i=0; i<M; i++)
            sb.wait ();
    }

    for (int i=0; i<f.size (); i++)
        f[i].get();


    for (int i=0; i<f.size (); i++)
        f[i] = async(launch::async, [&]()
        {
            lb.wait ();
            for (int i=0; i<M; i++)
                lb.wait ();
        });

    {
        lb.wait ();
        TRACE_PERF(str(boost::format("locking_barrier %d threads, %d times") % N % M));

        for (int i=0; i<M; i++)
            lb.wait ();
    }

    for (int i=0; i<f.size (); i++)
        f[i].get();
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

        evalate(10*concurentThreadsSupported);
        evalate((concurentThreadsSupported+1)/2);
    }
}
