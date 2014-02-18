Backtraces rationale
====================
- The Backtrace class should store a backtrace of the call stack in 1 ms on windows, os x and linux.

To make bugs go squish you want some sort of indication as to where it is. This is a bunch of small classes that makes use of runtime backtraces in C++ to decorate exceptions and segfaults with info about their origin. Each class header comment defines its expected behaviour. 


Utilities based on backtraces
-----------------------------
- The PrettifySegfault class should attempt to capture any null-pointer exception (SIGSEGV and SIGILL) in the program, log a backtrace, and then throw a regular C++ exception from the function causing the signal.
- The VolatilePtr class should guarantee thread-safe access to objects, with compile-time errors on missing locks and run-time exceptions with backtraces on deadlocks.
- The ExceptionAssert class should store details about an assertion that failed.


Examples
--------
Please refer to the static test() function in each class.


How to use
----------
The intention is not to build these snippets as a .lib, just include them in your source. They depend on the boost library.


License
-------
GPL v3.0


Other utilities
---------------
- Demangle should perform a system specific demangling of compiled C++ names.
- The DetectGdb class should detect whether the current process was started through, or is running through, gdb (or as a child of another process).
- The Timer class should measure time with a high accuracy.
- The TaskTimer class should log how long time it takes to execute a scope while distinguishing nested scopes and different threads.
