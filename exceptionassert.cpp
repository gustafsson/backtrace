#include "exceptionassert.h"

#include "tasktimer.h"
#include "expectexception.h"

#include <boost/format.hpp>

using namespace std;
using namespace boost;


ExceptionAssert::
        ExceptionAssert()
{
}


void ExceptionAssert::
        test()
{
    EXPECT_EXCEPTION(ExceptionAssert, EXCEPTION_ASSERT(false));
}


void ExceptionAssert::
        throwException(const char* functionMacro,
                       const char* fileMacro, int lineMacro,
                       const char* condition,
                       const std::string& callerMessage, int skipFrames)
{
    ::boost::exception_detail::throw_exception_(
        ExceptionAssert()
            << ExceptionAssert_condition(condition)
            << ExceptionAssert_message(callerMessage)
            << Backtrace::make(2 + skipFrames),
        functionMacro,
        fileMacro,
        lineMacro);
}


void ExceptionAssert::
        throwException(const char* functionMacro,
                    const char* fileMacro, int lineMacro,
                    const char* details,
                    const boost::format& f)
{
    throwException (functionMacro, fileMacro, lineMacro, details, f.str (), 1);
}


void ExceptionAssert::
        logAndThrow(const char* functionMacro,
                         const char* fileMacro, int lineMacro,
                         const char* condition,
                         const std::string& callerMessage, int skipFrames)
{
    logError(functionMacro, fileMacro, lineMacro, condition, callerMessage);
    throwException (functionMacro, fileMacro, lineMacro, condition, callerMessage, ++skipFrames);
}


void ExceptionAssert::
        logAndThrow(const char* functionMacro,
                    const char* fileMacro, int lineMacro,
                    const char* details,
                    const boost::format& f)
{
    logAndThrow (functionMacro, fileMacro, lineMacro, details, f.str ());
}


void ExceptionAssert::
        logError(const char* functionMacro,
                 const char* fileMacro, int lineMacro,
                 const char* condition,
                 const std::string& callerMessage)
{
    TaskInfo ti("ExceptionAssert");

    TaskInfo("%s:%d %s", fileMacro, lineMacro, functionMacro);
    TaskInfo("condition: %s", condition);
    TaskInfo("message: %s", callerMessage.c_str());

    TaskInfo("%s", Backtrace::make_string (2).c_str());
}


void ExceptionAssert::
        logError(const char* functionMacro,
                 const char* fileMacro, int lineMacro,
                 const char* details,
                 const boost::format& f)
{
    logError (functionMacro, fileMacro, lineMacro, details, f.str ());
}
