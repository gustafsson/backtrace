#include "detectgdb.h"

#ifndef _MSC_VER
#include <sys/types.h>
#include <sys/ptrace.h>
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
  int status;
  int res;

  if (pid == -1)
    {
      perror("fork");
      return -1;
    }

  if (pid == 0)
    {
      int ppid = getppid();

      /* Child */
      if (ptrace(PTRACE_ATTACH, ppid, NULL, NULL) == 0)
        {
          /* Wait for the parent to stop and continue it */
          waitpid(ppid, NULL, 0);
          ptrace(PTRACE_CONT, ppid, NULL, NULL);

          /* Detach */
          ptrace(PTRACE_DETACH, getppid(), NULL, NULL);

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
      waitpid(pid, &status, 0);
      res = WEXITSTATUS(status);
    }
  return res;
}


bool DetectGdb::
        is_running_through_gdb()
{
    bool is_attached_by_system_debugger = is_running_through_gdb_terminus();
    bool is_attached_in_qt_creator = is_running_through_gdb_xorl();

    return is_attached_by_system_debugger || is_attached_in_qt_creator;
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
