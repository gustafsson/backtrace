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

#### shared_state example ####
_The shared\_state<T> class is a smart pointer that should guarantee thread-safe access to objects of type T._

Make an instance of an object thread-safe by storing it in a shared\_state smart pointer.

````cpp
    class A {
    public:
      void foo();
      void bar() const;
    };
````

Then use the object like so:

````cpp
    // Read and write access
    {
      auto w = a.write();
      w->foo();
      w->bar();
    }

    // Read-only access
    {
      auto r = a.read();
      r->bar();
    }
````

.read() and .write() creates thread-safe critical sections for shared read-only access when using .read() and for mutually exclusive read-and-write access using .write(). They both throw exceptions on lock timeout, embed a backtrace in an exception object like this:

````cpp
template<class T>
class lock_failed_boost
  : public shared_state<T>::lock_failed
  , public virtual boost::exception
{};


template<>
struct shared_state_traits<A>
    : shared_state_traits_default
{
  template<class T>
  void timeout_failed () {
    this_thread::sleep_for (chrono::duration<double>{timeout()});

    BOOST_THROW_EXCEPTION(lock_failed_boost<T>{} << Backtrace::make ());
  }
};

... {
  shared_state<A> a(new A);
  ...
  try {
    auto w = a.write();
    ...
  } catch (lock_failed& x) {
    const Backtrace* backtrace = boost::get_error_info<Backtrace::info>(x);
    ...
  }
... }
````

See shared\_state.pdf or shared\_state.h for details.

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
