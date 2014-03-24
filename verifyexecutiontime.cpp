#include "verifyexecutiontime.h"
#include "tasktimer.h"
#include "backtrace.h"

using namespace std;

VerifyExecutionTime::report VerifyExecutionTime::default_report_func_;

void VerifyExecutionTime::
        default_report(float expected_time, float execution_time, const std::string& label)
{
    string expected = TaskTimer::timeToString( expected_time );
    string elapsed = TaskTimer::timeToString( execution_time );

    TaskInfo(boost::format("!!! VerifyExecutionTime: Took %s > %s %s")
             % elapsed % expected % label);
}


void VerifyExecutionTime::
        set_default_report( report report_func )
{
    default_report_func_ = report_func;
}


VerifyExecutionTime::ptr VerifyExecutionTime::
        start( float expected_time, report report_func )
{
    if (!report_func)
    {
        report_func = default_report_func_;
    }

    if (!report_func)
    {
        // It should print a backtrace by default if no report func is given.
        report_func = [](float expected_time, float execution_time){ default_report(expected_time, execution_time, Backtrace::make_string ()); };
    }

    return ptr(new VerifyExecutionTime( expected_time, report_func ));
}


VerifyExecutionTime::
        VerifyExecutionTime(float expected_time, report report_func)
    :
      expected_time_(expected_time),
      report_func_(report_func)
{
}


VerifyExecutionTime::
        ~VerifyExecutionTime()
{
    if (std::uncaught_exception())
        return;

    double execution_time = timer_.elapsed ();

    if (expected_time_ < execution_time)
    {
        report_func_(expected_time_, execution_time);
    }
}


//////////////////////////////////
// VerifyExecutionTime::test

#include "exceptionassert.h"
//#include <boost/thread.hpp>
#include <thread>

void VerifyExecutionTime::
        test()
{
    // It should warn if it takes longer than specified to execute a scope.
    {
        float expected_time=0.003, execution_time=0;

        {
            VerifyExecutionTime::ptr x = VerifyExecutionTime::start (expected_time, [&](float, float v){
                execution_time = v;
            });
            this_thread::sleep_for (chrono::milliseconds(1));
        }

        EXCEPTION_ASSERT_LESS(execution_time, expected_time);

        bool did_report = false;
        {
            VerifyExecutionTime::ptr x = VerifyExecutionTime::start (0.001, [&did_report](float,float){did_report = true;});
            this_thread::sleep_for (chrono::milliseconds(1));

            EXCEPTION_ASSERT(!did_report);
        }
        EXCEPTION_ASSERT(did_report);
    }

    // It should print a backtrace by default if no report func is given.
    {
        // See VerifyExecutionTime::start
    }

    // It should not warn about execution time if unwinding from an exception.
    {
        bool did_report = false;

        try {
            VerifyExecutionTime::ptr x = VerifyExecutionTime::start (0.001, [&did_report](float,float){did_report = true;});
            this_thread::sleep_for (chrono::milliseconds(2));
            throw 0;
        } catch (int) {}

        EXCEPTION_ASSERT(!did_report);
    }

    // It should cause an overhead of less than 1.5 microseconds in a release
    // build and less than 3 microseconds in a debug build.
    {
        bool debug = false;
#ifdef _DEBUG
        debug = true;
#endif

        int N = 100000;
        Timer t;
        for (int i=0; i<N; ++i) {
            VerifyExecutionTime(0.1);
        }
        double T = t.elapsedAndRestart () / N;

        EXCEPTION_ASSERT_LESS( T, debug ? 2e-6 : 0.8e-6 );

        for (int i=0; i<N; ++i) {
            VerifyExecutionTime::ptr a = VerifyExecutionTime::start (0.1);
        }
        T = t.elapsed () / N;

        EXCEPTION_ASSERT_LESS( T, debug ? 3e-6 : 1.5e-6 );
    }
}
