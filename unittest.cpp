#include "unittest.h"

#include "backtrace.h"
#include "exceptionassert.h"
#include "prettifysegfault.h"
#include "shared_state.h"
#include "tasktimer.h"
#include "timer.h"
#include "verifyexecutiontime.h"

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
        test()
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
    } catch (const exception& x) {
        TaskInfo(boost::format("%s") % boost::diagnostic_information(x));
        printf("\n FAILED in %s::test()\n\n", lastname.c_str ());
        return 1;
    } catch (...) {
        TaskInfo(boost::format("Not an std::exception\n%s") % boost::current_exception_diagnostic_information ());
        printf("\n FAILED in %s::test()\n\n", lastname.c_str ());
        return 1;
    }

    printf("\n OK\n\n");
    return 0;
}

} // namespace BacktraceTest
