#include "backtrace.h"
#include "exceptionassert.h"
#include "demangle.h"
#include "timer.h"

#ifdef __APPLE__
#include <iostream>
#include <stdio.h>
#endif

#ifndef _MSC_VER
#include <execinfo.h>
#else
#include "StackWalker.h"
#include "TlHelp32.h"
#endif

using namespace boost;
using namespace std;

typedef error_info<struct failed_condition,const char*> failed_condition_type;

void *bt_array[256];
size_t array_size;
void printSignalInfo(int sig);

#ifdef __APPLE__
string exec_get_output(string cmd) {
    FILE* pipe = popen(cmd.c_str (), "r");
    if (!pipe)
        return "";

    char buffer[128];
    string result = "";
    while(!feof(pipe)) {
        if(fgets(buffer, 128, pipe) != NULL)
            result += buffer;
    }
    pclose(pipe);
    return result;
}
#endif

void Backtrace::
        malloc_free_log()
{
    fflush(stdout);

#ifndef _MSC_VER
    // GCC supports malloc-free backtrace which is kind of neat
    // It does require the stack to grow though as this is a function call.

    // get void*'s for all entries on the stack
    array_size = backtrace(bt_array, 256);

    // print out all the frames to stderr
    backtrace_symbols_fd(bt_array, array_size, 2);
    fputs("\n",stderr);
#else
    // If we can't do a malloc-free backtrace. Just attempt a regular one and see what happens
    fputs(Backtrace::make_string ().c_str(), stderr);
    fputs("\n",stderr);
#endif

    fflush(stderr);
}

#if defined(_WIN32)

class StackWalkerString: private StackWalker
{
public:
    string getStackTrace(HANDLE hThread = GetCurrentThread())
    {
        fflush(stdout);
        if (!str_.empty())
            str_ += "\n\n";
        ShowCallstack(hThread);
        return str_;
    }

    string str() { return str_; }

private:
    virtual void OnOutput(LPCSTR szText)
    {
        TaskInfo("%s", szText);
        StackWalker::OnOutput(szText);
    }

    virtual void OnCallStackOutput(LPCSTR szText)
    {
        str_ += szText;
    }

    virtual void OnDbgHelpErr(LPCSTR szFuncName, DWORD gle, DWORD64 addr)
    {
        if (gle==487 && 0==strcmp(szFuncName, "SymGetLineFromAddr64"))
            ; // ignore
        else if (gle==487 && 0==strcmp(szFuncName, "SymGetSymFromAddr64"))
            ; // ignore
        else
            StackWalker::OnDbgHelpErr(szFuncName, gle, addr);
    }

    string str_;
};


string prettyBackTrace(int /*skipFrames*/)
{
    // http://stackoverflow.com/questions/590160/how-to-log-stack-frames-with-windows-x64
    // http://www.codeproject.com/Articles/11132/Walking-the-callstack
    // http://msdn.microsoft.com/en-us/library/windows/desktop/ms684335%28v=vs.85%29.aspx
    // http://stackoverflow.com/questions/9965784/how-to-obtain-list-of-thread-handles-from-a-win32-process
    StackWalkerString stw;

    stw.getStackTrace();

    DWORD currentProcessId = GetCurrentProcessId();
    DWORD currentThreadId = GetCurrentThreadId();
    HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (h != INVALID_HANDLE_VALUE) {
        THREADENTRY32 te;
        te.dwSize = sizeof(te);
        if (Thread32First(h, &te)) {
            do {
                if (te.dwSize >= FIELD_OFFSET(THREADENTRY32, th32OwnerProcessID) +
                        sizeof(te.th32OwnerProcessID)) {
                            if (currentProcessId == te.th32OwnerProcessID && currentThreadId != te.th32ThreadID)
                    {
                        HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, TRUE, te.th32ThreadID);
                        stw.getStackTrace(hThread);
                        CloseHandle(hThread);
                        fflush(stdout);
                    }
                }
                te.dwSize = sizeof(te);
            } while (Thread32Next(h, &te));
        }
        CloseHandle(h);
    }
    return stw.str();
}


Backtrace::info Backtrace::
        make(int /*skipFrames*/)
{
    Backtrace b;
    b.pretty_print_ = prettyBackTrace();
    return Backtrace::info(b);
}

string Backtrace::
        to_string() const
{
    return b.pretty_print_;
}

#else

Backtrace::info Backtrace::
    make(int skipFrames)
{
    Backtrace b;
    if (skipFrames < 0)
        skipFrames = 0;

    void *array[256];
    int size;

    // get void*'s for all entries on the stack
    size = backtrace(array, 256);

    if (size <= skipFrames)
        b.pretty_print_ = str(format("Backtrace::make(%s) failed") % skipFrames);
    else
        {
        b.frames_.resize (size-skipFrames);
        copy(array + skipFrames, array + size, b.frames_.begin ());
        }

    return Backtrace::info(b);
}


string Backtrace::
        to_string() const
{
    if (!pretty_print_.empty ())
        return pretty_print_;

    char** msg = backtrace_symbols(&frames_[0], frames_.size());
    if (0 == msg)
        return "Couldn't get backtrace symbol names for pretty print";


    string bt = str(format("backtrace (%d frames)\n") % (frames_.size ()));
    bool found_pretty = false;

#ifdef __APPLE__
    string addrs;
    for (unsigned i=0; i < frames_.size(); ++i)
    {
        string s = msg[i];
        string addr = s.substr (40, 18);
        addrs += addr + " ";
    }

    int id = getpid();
    string cmd = str(format("xcrun atos -p %1% %2%") % id % addrs);
    string op = exec_get_output(cmd);
    found_pretty = !op.empty();
    bt += op;
#endif

    if (!found_pretty)
    for (unsigned i=0; i < frames_.size (); ++i)
    {
        string s = msg[i];
        try
        {
#ifdef __APPLE__
            //string first = s.substr (4, 59-4);
            size_t n = s.find_first_of (' ', 60);
            string name = s.substr (59, n-59);
            string last = s.substr (n);
#else
            size_t n1 = s.find_last_of ('(');
            size_t n2 = s.find_last_of ('+');
            size_t n3 = s.find_last_of ('/');
            string name = s.substr (n1+1, n2-n1-1);
            string last = " (" + s.substr(n2) + " " + s.substr(n3+1,n1-n3-1);
#endif
            bt += str(format("%-5d%s%s\n") % i % demangle(name.c_str ()) % last );
        }
        catch(const std::exception&)
        {
            bt += s;
            bt += "\n";
        }
    }

    free(msg);

    bt += "\n";

    return bt;
}
#endif


string Backtrace::
        to_string()
{
    return pretty_print_ = ((const Backtrace*)this)->to_string();
}


std::string Backtrace::
        make_string(int skipframes)
{
    return make(skipframes).value ().to_string();
}


Backtrace::
        Backtrace()
{
}


static void throwfunction()
{
    BOOST_THROW_EXCEPTION(unknown_exception() << Backtrace::make ());
}


void Backtrace::
        test()
{
    // It should store a backtrace of the call stack in 1 ms.
    {
        Timer t;
        Backtrace::info backtrace = Backtrace::make ();
        float T = t.elapsed ();
        EXCEPTION_ASSERT_LESS( T, 0.001f );
        EXCEPTION_ASSERT_LESS( 0u, backtrace.value ().frames_.size() + backtrace.value ().pretty_print_.size() );
    }

    // It should work as error info to boost::exception
    {
        try {
            BOOST_THROW_EXCEPTION(unknown_exception() << Backtrace::make ());
        } catch (const std::exception&) {
        }
    }

    // It should translate to a pretty backtrace when asked for a string representation
    {
        do try {
            throwfunction();
        } catch (const boost::exception& x) {
            string s = diagnostic_information(x);

            try {
                EXCEPTION_ASSERTX( s.find ("throwfunction()") != string::npos, s );
                EXCEPTION_ASSERTX( s.find ("backtrace.cpp(276)") != string::npos, s );
                EXCEPTION_ASSERTX( s.find ("Backtrace::test()") != string::npos, s );
                EXCEPTION_ASSERTX( s.find ("start") != string::npos, s );
                // If atos is available
                EXCEPTION_ASSERTX( s.find ("(backtrace.cpp:276)") != string::npos, s );
    #ifdef _DEBUG
                // The call to throwfunction will be removed by optimization
                EXCEPTION_ASSERTX( s.find ("main") != string::npos, s );
                EXCEPTION_ASSERTX( s.find ("(backtrace.cpp:303)") != string::npos, s );
    #else
                EXCEPTION_ASSERTX( s.find ("(backtrace.cpp:303)") == string::npos, s );
    #endif
                break;
            } catch (const ExceptionAssert& e) {
                char const* const* condition = get_error_info<ExceptionAssert::ExceptionAssert_condition>(e);

                x << failed_condition_type(*condition);
            }
            throw;
        } while (false);
    }
}
