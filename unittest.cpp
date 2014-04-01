#include "unittest.h"

#include "backtrace.h"
#include "exceptionassert.h"
#include "prettifysegfault.h"
#include "shared_state.h"
#include "tasktimer.h"
#include "timer.h"
#include "verifyexecutiontime.h"
#include "demangle.h"
#include "barrier.h"

#include <stdio.h>
#include <exception>

#include <boost/exception/diagnostic_information.hpp>

using namespace std;

namespace BacktraceTest {

string lastname;

#define RUNTEST(x) do { \
        TaskTimer tt("%s", #x); \
        lastname = #x; \
        x::test (); \
    } while(false)

int UnitTest::
        test(bool rethrow_exceptions)
{
    try {
        Timer(); // Init performance counting
        TaskTimer tt("Running tests");

        RUNTEST(Backtrace);
        RUNTEST(ExceptionAssert);
        RUNTEST(PrettifySegfault);
        RUNTEST(Timer);
        RUNTEST(shared_state_test);
        RUNTEST(VerifyExecutionTime);
        RUNTEST(spinning_barrier);
        RUNTEST(locking_barrier);

    } catch (const exception& x) {
        if (rethrow_exceptions)
            throw;

        fflush(stdout);
        fprintf(stderr, "%s",
                str(boost::format("%s\n"
                                  "%s\n"
                                  " FAILED in %s::test()\n\n")
                    % vartype(x) % boost::diagnostic_information(x) % lastname ).c_str());
        fflush(stderr);
        return 1;
    } catch (...) {
        if (rethrow_exceptions)
            throw;

        fflush(stdout);
        fprintf(stderr, "%s",
                str(boost::format("Not an std::exception\n"
                                  "%s\n"
                                  " FAILED in %s::test()\n\n")
                    % boost::current_exception_diagnostic_information () % lastname ).c_str());
        fflush(stderr);
        return 1;
    }

    printf("\n OK\n\n");
    return 0;
}

} // namespace BacktraceTest
