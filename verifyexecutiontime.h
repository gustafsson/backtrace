#ifndef VERIFYEXECUTIONTIME_H
#define VERIFYEXECUTIONTIME_H

#include "timer.h"

#include <memory>
#include <functional>

/**
 * @brief The VerifyExecutionTime class should warn if it takes longer than
 * specified to execute a scope.
 *
 * It should print a backtrace by default if no report func is given.
 *
 * It should not warn about execution time if unwinding from an exception.
 *
 * It should cause an overhead of less than 1.5 microseconds in a release
 * build and less than 3 microseconds in a debug build.
 *
 * Never throw from the report function. Doing so results in undefined
 * behaviour.
 */
class VerifyExecutionTime
{
public:
    typedef std::shared_ptr<VerifyExecutionTime> ptr;
    typedef std::function<void( float expected_time, float execution_time )> report;

    static ptr start( float expected_time_, report func=0 );

    static void default_report( float expected_time, float execution_time, const std::string& label );
    static void set_default_report( report func );

    ~VerifyExecutionTime();

private:
    VerifyExecutionTime( float expected_time_, report func=0 );

    Timer timer_;
    float expected_time_;
    report report_func_;
    static report default_report_func_;

public:
    static void test();
};

#endif // VERIFYEXECUTIONTIME_H
