Backtraces rationale
====================
_The Backtrace class should store a backtrace of the call stack in 1 ms on windows, os x and linux._

To make bugs go squish you want some sort of indication as to where it is. This is a bunch of small classes that makes use of runtime backtraces in C++ to decorate exceptions and segfaults with info about their origin. Each class header comment defines its expected behaviour. 


#### Backtrace example ####

````cpp
string backtrace = Backtrace::make_string ();
cout << backtrace;
````

Example output (compiled with clang-500.2.79):

    backtrace (5 frames)
    Backtrace::make_string(int) (in backtrace-unittest) (backtrace.cpp:264)
    Backtrace::test() (in backtrace-unittest) (backtrace.cpp:283)
    BacktraceTest::UnitTest::test() (in backtrace-unittest) (unittest.cpp:34)
    start (in libdyld.dylib) + 1
    0x0000000000000001


#### PrettifySegfault example ####
_The PrettifySegfault class should attempt to capture any null-pointer exception (SIGSEGV and SIGILL) in the program, log a backtrace, and then throw a regular C++ exception from the function causing the signal._

````cpp
void nasty_function() {
  *(int*) NULL = 0;
}
````

Somewhere else

````cpp
    try {
      nasty_function();       
    } catch (const exception& x) {
      string backtrace_and_signal_name = boost::diagnostic_information(x);
    }
````

#### VolatilePtr example ####
_The VolatilePtr class should guarantee thread-safe access to objects, with compile-time errors on missing locks and run-time exceptions with backtraces on deadlocks._

Make an object thread-safe by inheriting from VolatilePtr and make all member variables private or protected.

````cpp
class A: public VolatilePtr<A>
{
public:
    void a (int v);
};
````

Then use the object like so:

````cpp
    A::Ptr mya (new A);
    
    // can't write to mya
    // compile time error: passing 'volatile A' as 'this' argument of 'void A::a (int)' discards qualifiers
    // mya->a (5);
    
    {
        // Lock for write access
        A::WritePtr w (mya);
        w->a (5);
        // Unlock on out-of-scope
    }
    
    // Lock for a single call
    A::WritePtr (mya)->a (5);
    write1(mya)->a (5);
````

The idea is to use a pointer to a volatile object when juggling references to objects. From a volatile object you can only access methods that are volatile (just like only const methods are accessible from a const object). Using the volatile classifier blocks access to use any "regular" (non-volatile) methods.


#### ExceptionAssert example ####
_The ExceptionAssert class should store details about an assertion that failed._

````cpp
    try {
         EXCEPTION_ASSERT_EQUALS( 1, 2 );
    } catch (const exception& x) {
         string what = boost::diagnostic_information(x);
    }
````

#### More examples ####
Please refer to the static test() function in each class for more complete examples.


## How to use this in your own code ##

The intention is not to build these snippets as a .lib, just include them in your source. They depend on the boost library.


## License ##

GPL v3.0


## Other utilities ##

- Demangle should perform a system specific demangling of compiled C++ names.
- The DetectGdb class should detect whether the current process was started through, or is running through, gdb (or as a child of another process).
- The Timer class should measure time with a high accuracy.
- The TaskTimer class should log how long time it takes to execute a scope while distinguishing nested scopes and different threads.
