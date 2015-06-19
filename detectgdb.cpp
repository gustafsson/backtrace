#include "detectgdb.h"

#ifndef _MSC_VER
    #include <sys/types.h>
    #ifndef DARWIN_NO_CARBON // skip for ios
        #include <sys/ptrace.h>
    #endif
    #include <sys/wait.h>
#else
    #include <Windows.h> // IsDebuggerPresent
#endif

#ifndef PTRACE_ATTACH
    #define PTRACE_ATTACH PT_ATTACH
    #define PTRACE_CONT PT_CONTINUE
    #define PTRACE_DETACH PT_DETACH
#endif

#include <stdio.h>
#include <errno.h>

#ifdef __APPLE__
    #include <TargetConditionals.h>
#endif

#if defined(__GNUC__)
    #include <unistd.h>
#else
    #define fileno _fileno
#endif

static bool was_started_through_gdb_ = DetectGdb::is_running_through_gdb ();

#ifndef _MSC_VER

// http://stackoverflow.com/a/10973747/1513411
// gdb apparently opens FD(s) 3,4,5 (whereas a typical program uses only stdin=0, stdout=1, stderr=2)
bool is_running_through_gdb_xorl()
{
    bool gdb = false;
    FILE *fd = fopen("/tmp", "r");

    if (fileno(fd) >= 5)
    {
        gdb = true;
    }

    fclose(fd);
    return gdb;
}


// http://stackoverflow.com/a/3599394/1513411
bool is_running_through_gdb_terminus()
{
  int pid = fork();
  int status = 0;
  int res;

  if (pid == -1)
    {
      printf("Fork failed!\n");
      perror("fork");
      return 1;
    }

  if (pid == 0)
    {
      /* Child fork */
      int ppid = getppid();

      if (ptrace(PTRACE_ATTACH, ppid, nullptr, 0) == 0)
        {
          /* Wait for the parent to stop and continue it */
          waitpid(ppid, nullptr, 0);
          ptrace(PTRACE_CONT, ppid, nullptr, 0);

          /* Detach */
          ptrace(PTRACE_DETACH, ppid, nullptr, 0);

          /* We were the tracers, so gdb is not present */
          res = 0;
        }
      else
        {
          /* Trace failed so gdb is present */
          res = 1;
        }

      _exit(res);
    }
  else
    {
      pid_t w = 0;
      do
        {
          // the first signal might be an unblocked signal, skip it
          w = waitpid(pid, &status, 0);
        }
      while (w < 0 && errno == EINTR);

      // fall-back to return "true" if the fork failed for whatever reason
      res = WIFEXITED(status) ? WEXITSTATUS(status) : 1;

        //std::cout << "WIFCONTINUED(status)="<< WIFCONTINUED(status)
        //        << ", WSTOPSIG(status)=" << WSTOPSIG(status) << std::endl;
        //std::cout << "WIFSTOPPED(status)="<< WIFSTOPPED(status)
        //        << ", WSTOPSIG(status)=" << WSTOPSIG(status) << std::endl;
        //std::cout << "WIFSIGNALED(status)="<< WIFSIGNALED(status)
        //        << ", WTERMSIG(status)=" << WTERMSIG(status) << std::endl;
        //std::cout << "WIFEXITED(status)="<< WIFEXITED(status)
        //        << ", WEXITSTATUS(status)=" << WEXITSTATUS(status) << std::endl;
    }
  return res;
}


bool DetectGdb::
        is_running_through_gdb()
{
#if defined(__APPLE_CPP__) && (TARGET_OS_IPHONE==0)
    // No implementation for detecting IOS debugger
    #ifdef _DEBUG
        return false;
    #else
        return true;
    #endif
#else
    bool is_attached_in_qt_creator = is_running_through_gdb_xorl();
    bool is_attached_by_system_debugger = is_running_through_gdb_terminus();
    return is_attached_in_qt_creator || is_attached_by_system_debugger;
#endif
}

#else

bool DetectGdb::
        is_running_through_gdb()
{
    return TRUE == IsDebuggerPresent();
}

#endif


bool DetectGdb::
        was_started_through_gdb()
{
    return was_started_through_gdb_;
}
