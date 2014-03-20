#pragma once

#include "timer.h"
#include <stdarg.h>
#if defined(__cplusplus) && !defined(__CUDACC__)
    #include <ostream>
#endif
#include <boost/format.hpp>

/**
The TaskTimer class should log how long time it takes to execute a scope while
distinguishing nested scopes and different threads.


A canonical example
-------------------
{
    TaskTimer tt("Doing this slow thing");
    doSlowThing();
}

Example output:

12:49:36.241581   Doing this slow thing... done in 100 ms.

Where "12:49:36.241581" is the time when creating TaskTimer and
"done in 100 ms." will be sent to cout when doSlowThing has
returned, and 'TaskTimer tt' is going out of scope.


Nested and formatted logging
----------------------------
{
    int N = 2;
    TaskTimer tt("Doing these %d slow things", N);
    for (int i=0; i<N; ++i) {
        TaskTimer tt(boost::format("Thing %d") % i);
        doSlowThing();
    }
}

Example output:
12:49:36.200000   Doing these 2 slow things
12:49:36.200000   - Thing 0... done in 100 ms.
12:49:36.300000   - Thing 1... done in 100 ms.
12:49:36.400000   done in 200 ms.


Running the previous example in two threads
-------------------------------------------
Example output:
12:49:36.200000   Doing these 2 slow things
12:49:36.200000   - Thing 0... done in 100 ms.
12:49:36.250000 1     Doing these 2 slow things
12:49:36.250000 1     - Thing 0... done in 100 ms.
12:49:36.300000   - Thing 1... done in 100 ms.
12:49:36.350000 1     - Thing 1... done in 100 ms.
12:49:36.400000   done in 200 ms.
12:49:36.450000 1     done in 200 ms.


Showing progress
----------------
{
    TaskTimer tt("Doing this slow thing");
    initialize();
    for (int i=0; i<9; ++i) {
        tt.partlyDone();
        doSlowThing();
    }
}

Where tt.partlyDone() will output an extra dot "." for each call.


Use TaskInfo to omit "done in 100 ms."
*/
class TaskTimer {
public:
    enum LogLevel {
        LogVerbose = 0,
        LogDetailed = 1,
        LogSimple = 2
    };

    TaskTimer(LogLevel logLevel, const char* task, ...);
    TaskTimer(bool, LogLevel logLevel, const char* task, va_list args);
    TaskTimer(const char* task, ...);
    TaskTimer(bool, const char* task, va_list args);
    TaskTimer(const boost::format& fmt);
    TaskTimer();
    TaskTimer(const TaskTimer&) = delete;
    TaskTimer& operator=(const TaskTimer&) = delete;
    ~TaskTimer();

    static void this_thread_quit();

    void info(const char* taskInfo, ...);
    void partlyDone();
    void suppressTiming();
    double elapsedTime();

    #if defined(__cplusplus) && !defined(__CUDACC__)
        static void setLogLevelStream( LogLevel logLevel, std::ostream* str );
        static bool isEnabled(LogLevel logLevel);
    #endif

    static bool enabled();
    static void setEnabled( bool );
    static std::string timeToString( double T );

private:
    Timer timer_{false};

    unsigned numPartlyDone;
    bool is_unwinding;
    bool suppressTimingInfo;
    LogLevel logLevel;

    TaskTimer* upperLevel; // obsolete

    //TaskTimer& getCurrentTimer();
    void init(LogLevel logLevel, const char* task, va_list args);
    void initEllipsis(LogLevel logLevel, const char* f, ...);
    void vinfo(const char* taskInfo, va_list args);
    void logprint(const char* txt);
    bool printIndentation();
};

class TaskInfo {
public:
    TaskInfo(const char* task, ...);
    TaskInfo(const boost::format&);
    TaskInfo(const TaskInfo&) = delete;
    TaskInfo& operator=(const TaskInfo&) = delete;
    ~TaskInfo();

    TaskTimer& tt() { return *tt_; }
private:
    TaskTimer* tt_;
};

#define TaskLogIfFalse(X) if (false == (X)) TaskInfo("! Not true: %s", #X)
#define TIME(x) do { TaskTimer tt("%s", #x); x; } while(false)
