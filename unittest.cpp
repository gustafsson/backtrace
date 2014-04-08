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
#include "shared_state_traits_backtrace.h"

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
        RUNTEST(shared_state_traits_backtrace);

    } catch (const ExceptionAssert& x) {
        if (rethrow_exceptions)
            throw;

        char const * const * f = boost::get_error_info<boost::throw_file>(x);
        int const * l = boost::get_error_info<boost::throw_line>(x);
        char const * const * c = boost::get_error_info<ExceptionAssert::ExceptionAssert_condition>(x);
        std::string const * m = boost::get_error_info<ExceptionAssert::ExceptionAssert_message>(x);

        fflush(stdout);
        fprintf(stderr, "%s",
                str(boost::format("%s:%d: %s. %s\n"
                                  "%s\n"
                                  " FAILED in %s::test()\n\n")
                    % (f?*f:0) % (l?*l:-1) % (c?*c:0) % (m?*m:0) % boost::diagnostic_information(x) % lastname ).c_str());
        fflush(stderr);
        return 1;
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
