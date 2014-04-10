#include "prettifysegfault.h"
#include "backtrace.h"
#include "signalname.h"
#include "expectexception.h"
#include "tasktimer.h"
#include "exceptionassert.h"

#include <signal.h>

#ifdef _MSC_VER
#include <Windows.h>
#endif

bool is_doing_signal_handling = false;
bool has_caught_any_signal_value = false;
bool enable_signal_print = true;
bool nested_signal_handling = false;
int last_caught_signal = 0;

void handler(int sig);
void printSignalInfo(int sig, bool);

#ifndef _MSC_VER
void seghandle_userspace() {
  // note: because we set up a proper stackframe,
  // unwinding is safe from here.
  // also, by the time this function runs, the
  // operating system is "done" with the signal.

  // choose language-appropriate throw instruction
  // raise new MemoryAccessError "Segmentation Fault";
  // throw new MemoryAccessException;
  // longjmp(erroneous_exit);
  // asm { int 3; }
  // *(int*) NULL = 0;

    if (!nested_signal_handling)
        printSignalInfo(last_caught_signal, false);
}


void seghandle(int sig, __siginfo*, void* unused)
{    
  nested_signal_handling = is_doing_signal_handling;
  has_caught_any_signal_value = true;
  is_doing_signal_handling = true;
  last_caught_signal = sig;

  fflush(stdout);
  flush(std::cout);
  flush(std::cerr);
  if (enable_signal_print)
    fprintf(stderr, "\nError: signal %s(%d) %s\n", SignalName::name (sig), sig, SignalName::desc (sig));
  fflush(stderr);

  // http://feepingcreature.github.io/handling.html
  ucontext_t* uc = (ucontext_t*) unused;
  // No. I refuse to use triple-pointers.
  // Just pretend ref a = v; is V* ap = &v;
  // and then substitute a with (*ap).
//  ref gregs = uc->uc_mcontext.gregs;
//  ref eip = (void*) gregs[X86Registers.EIP],
//  ref esp = (void**) gregs[X86Registers.ESP];

  // 32-bit
//  void*& eip = (void*&)uc->uc_mcontext->__ss.__eip;
//  void**& esp = (void**&)uc->uc_mcontext->__ss.__esp;

  // 64-bit
  // warning: dereferencing type-punned pointer will break strict-aliasing rules
  // how should this be handled?
  void*& eip = (void*&)uc->uc_mcontext->__ss.__rip;
  void**& esp = (void**&)uc->uc_mcontext->__ss.__rsp;

  // imitate the effects of "call seghandle_userspace"
  esp --; // decrement stackpointer.
          // remember: stack grows down!
  *esp = eip;

  // set up OS for call via return, like in the attack
  eip = (void*) &seghandle_userspace;

  // this is reached because 'ret' jumps to seghandle_userspace
  is_doing_signal_handling = false;
}


void setup_signal_survivor(int sig)
{
  struct sigaction sa;
  sa.sa_flags = SA_SIGINFO;
  sigemptyset (&sa.sa_mask);
  sa.__sigaction_u.__sa_sigaction = &seghandle;
  bool sigsegv_handler = sigaction(sig, &sa, NULL) != -1;
  EXCEPTION_ASSERTX(sigsegv_handler, "failed to setup SIGSEGV handler");
}
#endif

void handler(int sig)
{
    nested_signal_handling = is_doing_signal_handling;
    has_caught_any_signal_value = true;
    is_doing_signal_handling = true;
    last_caught_signal = sig;

    // The intention is that handler(sig) should cause very few stack and heap
    // allocations. And 'printSignalInfo' should prints prettier info but with
    // the expense of a whole bunch of both stack and heap allocations.

    // http://feepingcreature.github.io/handling.html does not work, see note 4.

    fflush(stdout);
    if (enable_signal_print)
        fprintf(stderr, "\nError: signal %s(%d) %s\n", SignalName::name (sig), sig, SignalName::desc (sig));
    fflush(stderr);

    if (enable_signal_print)
        Backtrace::malloc_free_log ();

    if (!nested_signal_handling)
        printSignalInfo(sig, true);

    // This will not be reached if an exception is thrown
    is_doing_signal_handling = false;
}


void printSignalInfo(int sig, bool noaction)
{
    // Lots of memory allocations here. Not neat, but more helpful.

    if (enable_signal_print)
        TaskInfo("Got %s(%d) '%s'\n%s",
             SignalName::name (sig), sig, SignalName::desc (sig),
             Backtrace::make_string ().c_str());
    fflush(stdout);

    switch(sig)
    {
#ifndef _MSC_VER
    case SIGCHLD:
        return;

    case SIGWINCH:
        TaskInfo("Got SIGWINCH, could reload OpenGL resources here");
        fflush(stdout);
        return;
#endif
    case SIGABRT:
        TaskInfo("Got SIGABRT");
        fflush(stdout);
        if (!noaction)
            exit(1);
        return;

    case SIGILL:
    case SIGSEGV:
        if (enable_signal_print)
            TaskInfo("Throwing segfault_sigill_exception");
        fflush(stdout);

        if (!noaction)
            BOOST_THROW_EXCEPTION(segfault_sigill_exception()
                              << signal_exception::signal(sig)
                              << signal_exception::signalname(SignalName::name (sig))
                              << signal_exception::signaldesc(SignalName::desc (sig))
                              << Backtrace::make (2));
        return;

    default:
        if (enable_signal_print)
            TaskInfo("Throwing signal_exception");
        fflush(stdout);

        if (!noaction)
            BOOST_THROW_EXCEPTION(signal_exception()
                              << signal_exception::signal(sig)
                              << signal_exception::signalname(SignalName::name (sig))
                              << signal_exception::signaldesc(SignalName::desc (sig))
                              << Backtrace::make (2));
        return;
    }
}


#ifdef _MSC_VER

const char* ExceptionCodeName(DWORD code)
{
    switch(code)
    {
    case WAIT_IO_COMPLETION: return "WAIT_IO_COMPLETION / STATUS_USER_APC";
    case STILL_ACTIVE: return "STILL_ACTIVE, STATUS_PENDING";
    case EXCEPTION_ACCESS_VIOLATION: return "EXCEPTION_ACCESS_VIOLATION";
    case EXCEPTION_DATATYPE_MISALIGNMENT: return "EXCEPTION_DATATYPE_MISALIGNMENT";
    case EXCEPTION_BREAKPOINT: return "EXCEPTION_BREAKPOINT";
    case EXCEPTION_SINGLE_STEP: return "EXCEPTION_SINGLE_STEP";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
    case EXCEPTION_FLT_DENORMAL_OPERAND: return "EXCEPTION_FLT_DENORMAL_OPERAND";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
    case EXCEPTION_FLT_INEXACT_RESULT: return "EXCEPTION_FLT_INEXACT_RESULT";
    case EXCEPTION_FLT_INVALID_OPERATION: return "EXCEPTION_FLT_INVALID_OPERATION";
    case EXCEPTION_FLT_OVERFLOW: return "EXCEPTION_FLT_OVERFLOW";
    case EXCEPTION_FLT_STACK_CHECK: return "EXCEPTION_FLT_STACK_CHECK";
    case EXCEPTION_FLT_UNDERFLOW: return "EXCEPTION_FLT_UNDERFLOW";
    case EXCEPTION_INT_DIVIDE_BY_ZERO: return "EXCEPTION_INT_DIVIDE_BY_ZERO";
    case EXCEPTION_INT_OVERFLOW: return "EXCEPTION_INT_OVERFLOW";
    case EXCEPTION_PRIV_INSTRUCTION: return "EXCEPTION_PRIV_INSTRUCTION";
    case EXCEPTION_IN_PAGE_ERROR: return "EXCEPTION_IN_PAGE_ERROR";
    case EXCEPTION_ILLEGAL_INSTRUCTION: return "EXCEPTION_ILLEGAL_INSTRUCTION";
    case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
    case EXCEPTION_STACK_OVERFLOW: return "EXCEPTION_STACK_OVERFLOW";
    case EXCEPTION_INVALID_DISPOSITION: return "EXCEPTION_INVALID_DISPOSITION";
    case EXCEPTION_GUARD_PAGE: return "EXCEPTION_GUARD_PAGE";
    case EXCEPTION_INVALID_HANDLE: return "EXCEPTION_INVALID_HANDLE";
#ifdef STATUS_POSSIBLE_DEADLOCK
    case EXCEPTION_POSSIBLE_DEADLOCK: return "EXCEPTION_POSSIBLE_DEADLOCK";
#endif
    case CONTROL_C_EXIT: return "CONTROL_C_EXIT";
    default: return "UNKNOWN";
    }
}
const char* ExceptionCodeDescription(DWORD code)
{
    switch(code)
    {
    case EXCEPTION_ACCESS_VIOLATION: return "The thread tried to read from or write to a virtual address for which it does not have the appropriate access.";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "The thread tried to access an array element that is out of bounds and the underlying hardware supports bounds checking.";
    case EXCEPTION_BREAKPOINT: return "A breakpoint was encountered.";
    case EXCEPTION_DATATYPE_MISALIGNMENT: return "The thread tried to read or write data that is misaligned on hardware that does not provide alignment. For example, 16-bit values must be aligned on 2-byte boundaries; 32-bit values on 4-byte boundaries, and so on.";
    case EXCEPTION_FLT_DENORMAL_OPERAND: return "One of the operands in a floating-point operation is denormal. A denormal value is one that is too small to represent as a standard floating-point value.";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "The thread tried to divide a floating-point value by a floating-point divisor of zero.";
    case EXCEPTION_FLT_INEXACT_RESULT: return "The result of a floating-point operation cannot be represented exactly as a decimal fraction.";
    case EXCEPTION_FLT_INVALID_OPERATION: return "This exception represents any floating-point exception not included in this list.";
    case EXCEPTION_FLT_OVERFLOW: return "The exponent of a floating-point operation is greater than the magnitude allowed by the corresponding type.";
    case EXCEPTION_FLT_STACK_CHECK: return "The stack overflowed or underflowed as the result of a floating-point operation.";
    case EXCEPTION_FLT_UNDERFLOW: return "The exponent of a floating-point operation is less than the magnitude allowed by the corresponding type.";
    case EXCEPTION_ILLEGAL_INSTRUCTION: return "The thread tried to execute an invalid instruction.";
    case EXCEPTION_IN_PAGE_ERROR: return "The thread tried to access a page that was not present, and the system was unable to load the page. For example, this exception might occur if a network connection is lost while running a program over the network.";
    case EXCEPTION_INT_DIVIDE_BY_ZERO: return "The thread tried to divide an integer value by an integer divisor of zero.";
    case EXCEPTION_INT_OVERFLOW: return "The result of an integer operation caused a carry out of the most significant bit of the result.";
    case EXCEPTION_INVALID_DISPOSITION: return "An exception handler returned an invalid disposition to the exception dispatcher. Programmers using a high-level language such as C should never encounter this exception.";
    case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "The thread tried to continue execution after a noncontinuable exception occurred.";
    case EXCEPTION_PRIV_INSTRUCTION: return "The thread tried to execute an instruction whose operation is not allowed in the current machine mode.";
    case EXCEPTION_SINGLE_STEP: return "A trace trap or other single-instruction mechanism signaled that one instruction has been executed.";
    case EXCEPTION_STACK_OVERFLOW: return "The thread used up its stack.";
    default: return "No description";
    }
}

LONG WINAPI MyUnhandledExceptionFilter(
  _In_  struct _EXCEPTION_POINTERS *ExceptionInfo
)
{
    DWORD code = ExceptionInfo->ExceptionRecord->ExceptionCode;
    if (enable_signal_print)
        TaskInfo("Caught UnhandledException %s(0x%x) %s", ExceptionCodeName(code), code, ExceptionCodeDescription(code));

    // Translate from Windows Structured Exception to C signal.
    //C signals known in Windows:
    // http://msdn.microsoft.com/en-us/library/xdkz3x12(v=vs.110).aspx
    //#define SIGINT          2       /* interrupt */
    //#define SIGILL          4       /* illegal instruction - invalid function image */
    //#define SIGFPE          8       /* floating point exception */
    //#define SIGSEGV         11      /* segment violation */
    //#define SIGTERM         15      /* Software termination signal from kill */
    //#define SIGBREAK        21      /* Ctrl-Break sequence */
    //#define SIGABRT         22      /* abnormal termination triggered by abort call */

    //#define SIGABRT_COMPAT  6       /* SIGABRT compatible with other platforms, same as SIGABRT */

    // http://msdn.microsoft.com/en-us/library/windows/desktop/aa363082(v=vs.85).aspx
    int sig=0;
    switch (ExceptionInfo->ExceptionRecord->ExceptionCode)
    {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_DATATYPE_MISALIGNMENT:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case EXCEPTION_PRIV_INSTRUCTION:
    case EXCEPTION_IN_PAGE_ERROR:
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
    case EXCEPTION_STACK_OVERFLOW:
    case EXCEPTION_INVALID_DISPOSITION:
    case EXCEPTION_GUARD_PAGE:
    case EXCEPTION_INVALID_HANDLE:
#ifdef STATUS_POSSIBLE_DEADLOCK
    case EXCEPTION_POSSIBLE_DEADLOCK:
#endif
    case CONTROL_C_EXIT:
        sig = SIGSEGV;
        break;
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        sig = SIGILL;
        break;
    case EXCEPTION_FLT_DENORMAL_OPERAND:
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_INEXACT_RESULT:
    case EXCEPTION_FLT_INVALID_OPERATION:
    case EXCEPTION_FLT_OVERFLOW:
    case EXCEPTION_FLT_STACK_CHECK:
    case EXCEPTION_FLT_UNDERFLOW:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_INT_OVERFLOW:
        sig = SIGFPE;
        break;
    case EXCEPTION_BREAKPOINT:
    case EXCEPTION_SINGLE_STEP:
    default:
        break;
    }

    if (sig) {
        fflush(stdout);
        fflush(stderr);

        if (enable_signal_print)
            Backtrace::malloc_free_log ();

        printSignalInfo(sig, false);
        // unreachable, printSignalInfo throws a C++ exception for SIGFPE, SIGSEGV and SIGILL
    }

    // carry on with default exception handling
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif // _MSC_VER

void my_terminate() {
    std::cerr << ("\n\n"
                  "std::terminate was called with " + Backtrace::make_string ()) << std::endl;
    std::abort ();
}

void PrettifySegfault::
        setup ()
{
#ifndef _MSC_VER
    // subscribe to everything SIGSEGV and friends
    for (int i=1; i<=SIGUSR2; ++i)
    {
        switch(i)
        {
        case SIGCHLD:
            break;
        case SIGILL:
        case SIGSEGV:
            setup_signal_survivor(i);
            break;
        default:
            if (0!=strcmp(SignalName::name (i),"UNKNOWN"))
                signal(i, handler); // install our handler
            break;
        }
    }
#else
    SetUnhandledExceptionFilter(MyUnhandledExceptionFilter);
#endif

    std::set_terminate (my_terminate);
}


PrettifySegfault::SignalHandlingState PrettifySegfault::
        signal_handling_state()
{
    return is_doing_signal_handling ? PrettifySegfault::doing_signal_handling : PrettifySegfault::normal_execution;
}


void PrettifySegfault::
        EnableDirectPrint(bool enable)
{
    enable_signal_print = enable;
}


#include "detectgdb.h"


#if BOOST_CLANG
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnull-dereference"
#pragma clang diagnostic ignored "-Wself-assign"
#endif

void throw_catchable_segfault_exception()
{
    *(int*) NULL = 0;
    throw std::exception();
}


void throw_catchable_segfault_exception2()
{
    std::vector<std::vector<int> >(); // create and destroy a complex type in the same scope
    int a[100];
    memset(a, 0, sizeof(a));
    a[0] = a[1] + 10;

    *(int*) NULL = 0;

    printf("never reached %d\n", a[0]);
}


void throw_catchable_segfault_exception3()
{
    *(int*) NULL = 0;
}


void throw_catchable_segfault_exception4()
{
    int a=0;
    a=a;
    *(int*) NULL = 0;
}


class breaks_RAII_assumptions {
public:
    breaks_RAII_assumptions() {
        constructor_was_called = true;
    }
    ~breaks_RAII_assumptions() {
        destructor_was_called = true;
    }

    static bool constructor_was_called;
    static bool destructor_was_called;
};

bool breaks_RAII_assumptions::constructor_was_called = false;
bool breaks_RAII_assumptions::destructor_was_called = false;

void throw_catchable_segfault_exception5()
{
    breaks_RAII_assumptions tst;
    std::vector<std::vector<int> > leaking_memory(10); // the destructor will never be called
    *(int*) NULL = 0;
}



#if BOOST_CLANG
#pragma clang diagnostic pop
#endif

void throw_catchable_segfault_exception_noinline();
void throw_catchable_segfault_exception2_noinline();
void throw_catchable_segfault_exception3_noinline();
void throw_catchable_segfault_exception4_noinline();
void throw_catchable_segfault_exception5_noinline();


void PrettifySegfault::
        test()
{
    // Skip test if running through gdb
    if (DetectGdb::was_started_through_gdb ()) {
        TaskInfo("Running as child process, skipping PrettifySegfault test");
        return;
    }

    // It should attempt to capture any null-pointer exception (SIGSEGV and
    // SIGILL) in the program, log a backtrace, and then throw a regular C++
    // exception from the location causing the signal.
    {
        enable_signal_print = false;

        // In order for the EXPECT_EXCEPTION macro to catch the exception the call
        // must not be inlined as the function causing the signal will first return and
        // then throw the exception.
#ifdef _DEBUG
        EXPECT_EXCEPTION(segfault_sigill_exception, throw_catchable_segfault_exception());
        EXPECT_EXCEPTION(segfault_sigill_exception, throw_catchable_segfault_exception2());
        EXPECT_EXCEPTION(segfault_sigill_exception, throw_catchable_segfault_exception3());
        EXPECT_EXCEPTION(segfault_sigill_exception, throw_catchable_segfault_exception4());
        EXPECT_EXCEPTION(segfault_sigill_exception, throw_catchable_segfault_exception5());
#else
        EXPECT_EXCEPTION(segfault_sigill_exception, throw_catchable_segfault_exception_noinline());
        EXPECT_EXCEPTION(segfault_sigill_exception, throw_catchable_segfault_exception2_noinline());
        EXPECT_EXCEPTION(segfault_sigill_exception, throw_catchable_segfault_exception3_noinline());
        EXPECT_EXCEPTION(segfault_sigill_exception, throw_catchable_segfault_exception4_noinline());
        EXPECT_EXCEPTION(segfault_sigill_exception, throw_catchable_segfault_exception5_noinline());
#endif

        enable_signal_print = true;
    }

    // It returns immediately from the signalling function without unwinding
    // that scope and thus break the RAII assumption that a destructor will
    // always be called (does not apply in windows)
    {
        EXCEPTION_ASSERT(breaks_RAII_assumptions::constructor_was_called);
#ifndef _MSC_VER
        EXCEPTION_ASSERT(!breaks_RAII_assumptions::destructor_was_called);
        breaks_RAII_assumptions(); // This calls the destructor
#endif
        EXCEPTION_ASSERT(breaks_RAII_assumptions::destructor_was_called);
    }
}
