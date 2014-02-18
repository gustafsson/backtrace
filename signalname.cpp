#include "signalname.h"

#include <signal.h>

const char* SignalName::
        name(int signal)
{
    switch(signal)
    {
#ifndef _MSC_VER
    case SIGHUP:    return "SIGHUP";    // 1, hangup
#endif
    case SIGINT:	return "SIGINT";	// 2, interrupt
#ifndef _MSC_VER
    case SIGQUIT:	return "SIGQUIT";	// 3, quit
#endif
    case SIGILL:	return "SIGILL";	// 4, illegal instruction (not reset when caught)
#ifndef _MSC_VER
    case SIGTRAP:	return "SIGTRAP";	// 5, trace trap (not reset when caught)
#endif
    case SIGABRT:	return "SIGABRT";	// 6, abort()
    #if  (defined(_POSIX_C_SOURCE) && !defined(_DARWIN_C_SOURCE))
    case SIGPOLL:	return "SIGPOLL";	// 7, pollable event ([XSR] generated, not supported)
    #else	//  (!_POSIX_C_SOURCE || _DARWIN_C_SOURCE)
#ifndef _MSC_VER
    case SIGEMT:	return "SIGEMT";	// 7, EMT instruction
#endif
    #endif	//  (!_POSIX_C_SOURCE || _DARWIN_C_SOURCE)
    case SIGFPE:	return "SIGFPE";	// 8, floating point exception
#ifndef _MSC_VER
    case SIGKILL:	return "SIGKILL";	// 9, kill (cannot be caught or ignored)
    case SIGBUS:	return "SIGBUS";	// 10, bus error
#endif
    case SIGSEGV:	return "SIGSEGV";	// 11, segmentation violation
#ifndef _MSC_VER
    case SIGSYS:	return "SIGSYS";	// 12, bad argument to system call
    case SIGPIPE:	return "SIGPIPE";	// 13, write on a pipe with no one to read it
    case SIGALRM:	return "SIGALRM";	// 14, alarm clock
#endif
    case SIGTERM:	return "SIGTERM";	// 15, software termination signal from kill
#ifndef _MSC_VER
    case SIGURG:	return "SIGURG";	// 16, urgent condition on IO channel
    case SIGSTOP:	return "SIGSTOP";	// 17, sendable stop signal not from tty
    case SIGTSTP:	return "SIGTSTP";	// 18, stop signal from tty
    case SIGCONT:	return "SIGCONT";	// 19, continue a stopped process
    case SIGCHLD:	return "SIGCHLD";	// 20, to parent on child stop or exit
    case SIGTTIN:	return "SIGTTIN";	// 21, to readers pgrp upon background tty read
    case SIGTTOU:	return "SIGTTOU";	// 22, like TTIN for output if (tp->t_local&LTOSTOP)
    #if  (!defined(_POSIX_C_SOURCE) || defined(_DARWIN_C_SOURCE))
    case SIGIO:	return "SIGIO";	// 23, input/output possible signal
    #endif
    case SIGXCPU:	return "SIGXCPU";	// 24, exceeded CPU time limit
    case SIGXFSZ:	return "SIGXFSZ";	// 25, exceeded file size limit
    case SIGVTALRM:	return "SIGVTALRM";	// 26, virtual time alarm
    case SIGPROF:	return "SIGPROF";	// 27, profiling time alarm
    #if  (!defined(_POSIX_C_SOURCE) || defined(_DARWIN_C_SOURCE))
    case SIGWINCH:	return "SIGWINCH";	// 28, window size changes
    case SIGINFO:	return "SIGINFO";	// 29, information request
    #endif
    case SIGUSR1:   return "SIGUSR1";   // 30, user defined signal 1
    case SIGUSR2:   return "SIGUSR2";	// 31, user defined signal 2
#endif
    default:        return "UNKNOWN";
    }
}


const char* SignalName::
        desc(int signal)
{
    switch(signal)
    {
#ifndef _MSC_VER
    case SIGHUP:	return "hangup";	// 1
#endif
    case SIGINT:	return "interrupt";	// 2
#ifndef _MSC_VER
    case SIGQUIT:	return "quit";	// 3
#endif
    case SIGILL:	return "illegal instruction (not reset when caught)";	// 4
#ifndef _MSC_VER
    case SIGTRAP:	return "trace trap (not reset when caught)";	// 5
#endif
    case SIGABRT:	return "abort()";	// 6
    #if  (defined(_POSIX_C_SOURCE) && !defined(_DARWIN_C_SOURCE))
    case SIGPOLL:	return "pollable event ([XSR] generated, not supported)";	// 7
    #else	/* (!_POSIX_C_SOURCE || _DARWIN_C_SOURCE) */
#ifndef _MSC_VER
    case SIGEMT:	return "EMT instruction";	// 7
#endif
    #endif	/* (!_POSIX_C_SOURCE || _DARWIN_C_SOURCE) */
    case SIGFPE:	return "floating point exception";	// 8
#ifndef _MSC_VER
    case SIGKILL:	return "kill (cannot be caught or ignored)";	// 9
    case SIGBUS:	return "bus error";	// 10
#endif
    case SIGSEGV:	return "segmentation violation";	// 11
#ifndef _MSC_VER
    case SIGSYS:	return "bad argument to system call";	// 12
    case SIGPIPE:	return "write on a pipe with no one to read it";	// 13
    case SIGALRM:	return "alarm clock";	// 14
#endif
    case SIGTERM:	return "software termination signal from kill";	// 15
#ifndef _MSC_VER
    case SIGURG:	return "urgent condition on IO channel";	// 16
    case SIGSTOP:	return "sendable stop signal not from tty";	// 17
    case SIGTSTP:	return "stop signal from tty";	// 18
    case SIGCONT:	return "continue a stopped process";	// 19
    case SIGCHLD:	return "to parent on child stop or exit";	// 20
    case SIGTTIN:	return "to readers pgrp upon background tty read";	// 21
    case SIGTTOU:	return "like TTIN for output if (tp->t_local&LTOSTOP)";	// 22
    #if  (!defined(_POSIX_C_SOURCE) || defined(_DARWIN_C_SOURCE))
    case SIGIO:     return "input/output possible signal";	// 23
    #endif
    case SIGXCPU:	return "exceeded CPU time limit";	// 24
    case SIGXFSZ:	return "exceeded file size limit";	// 25
    case SIGVTALRM:	return "virtual time alarm";	// 26
    case SIGPROF:	return "profiling time alarm";	// 27
    #if  (!defined(_POSIX_C_SOURCE) || defined(_DARWIN_C_SOURCE))
    case SIGWINCH:	return "window size changes";	// 28
    case SIGINFO:	return "information request";	// 29
    #endif
    case SIGUSR1:   return "user defined signal 1"; // 30
    case SIGUSR2:   return "user defined signal 2"; // 31
#endif
    default:        return "unknown";
    }
}
