#include "timer.h"
#include "trace_perf.h"

#ifdef _MSC_VER
#include <Windows.h>
#endif

using namespace boost::posix_time;

Timer::Timer()
{
    restart();
}


void Timer::restart()
{
#ifdef _MSC_VER
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    start_ = li.QuadPart;
#else
    start_ = microsec_clock::local_time();
#endif
}


double Timer::elapsed() const
{
#ifdef _MSC_VER
    LARGE_INTEGER li;
    static double PCfreq = 1;
    for(static bool doOnce=true;doOnce;doOnce=false)
    {
        QueryPerformanceFrequency(&li);
        PCfreq = double(li.QuadPart);
    }
    QueryPerformanceCounter(&li);
    return double(li.QuadPart-start_)/PCfreq;
#else
    time_duration diff = microsec_clock::local_time() - start_;
    return diff.total_microseconds() * 1e-6;
#endif
}


double Timer::elapsedAndRestart()
{
#ifdef _MSC_VER
    LARGE_INTEGER li;
    static double PCfreq = 1;
    for(static bool doOnce=true;doOnce;doOnce=false)
    {
        QueryPerformanceFrequency(&li);
        PCfreq = double(li.QuadPart);
    }
    QueryPerformanceCounter(&li);
    __int64 now = li.QuadPart;
    double diff = double(now-start_)/PCfreq;
    start_ = now;
    return diff;
#else
    boost::posix_time::ptime now = microsec_clock::local_time();
    time_duration diff = now - start_;
    start_ = now;
    return diff.total_microseconds() * 1e-6;
#endif
}


void Timer::
        test()
{
    // It should measure duration with a high accuracy
    // (at least 0.1-millisecond resolution)
    {
        TRACE_PERF("it should measure short intervals as short");

        trace_perf_.reset ("it should have a low overhead");

        {Timer t;t.elapsed ();}
    }

    // It should have an overhead less than 0.8 microseconds when inactive in a
    // 'release' build (inacive in the sense that an instance is created but no
    // method is called).
    {
        TRACE_PERF("it should have a low overhead 10000");

        for (int i=0;i<10000;i++) {
            Timer t0;
            t0.elapsed ();
        }

        trace_perf_.reset ("it should produce stable measures 10000");

        for (int i=0;i<10000;i++) {
            Timer t0;
            t0.elapsed ();
        }
    }
}
