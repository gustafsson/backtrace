#include "shared_state.h"
#include "exceptionassert.h"
#include "expectexception.h"
#include "trace_perf.h"

#include <thread>
#include <future>

// scroll down to void shared_state_test::test () to see more examples

using namespace std;

class OrderOfOperationsCheck
{
public:
    const OrderOfOperationsCheck& operator= (int a) {
        a_ = a;
        return *this;
    }

private:
    int a_;
};


class InnerDestructor {
public:
    InnerDestructor(bool* inner_destructor):inner_destructor(inner_destructor) {}
    ~InnerDestructor() { *inner_destructor = true; }

private:
    bool* inner_destructor;
};


class ThrowInConstructor {
public:
    ThrowInConstructor(bool*outer_destructor, bool*inner_destructor):inner(inner_destructor) {
        throw 1;
    }

    ~ThrowInConstructor() { *outer_destructor = true; }

private:
    bool* outer_destructor;
    InnerDestructor inner;
};


class A
{
public:
    typedef shared_state<A> ptr;
    typedef shared_state<const A> const_ptr;

    A () {}
    A (const A& b) { *this = b; }
    A& operator= (const A& b);

    int     const_method () const { return a_; }
    void    method (int v) { a_ = v; }

    void    volatile_member_method () volatile;
    void    consttest () const volatile;

    int     noinlinecall() const { cout << ""; return 0; }

private:

    int a_;
    OrderOfOperationsCheck c_;
};


template<>
class shared_state_traits<A> {
public:
    int timeout_ms() { return 1; }
    int verify_execution_time_ms() { return -1; }
    VerifyExecutionTime::report report_func() { return 0; }
};


class B
{
public:
    class shared_state_traits {
    public:
        int timeout_ms() { return 10; }
        int verify_execution_time_ms() { return -1; }
        VerifyExecutionTime::report report_func() { return 0; }
    };

    typedef shared_state<B> ptr;

    int work_a_lot(int i) const;
};


class C {};


class with_timeout_0
{
public:
    typedef shared_state<with_timeout_0> ptr;
    typedef shared_state<const with_timeout_0> const_ptr;
};


template<>
class shared_state_traits<with_timeout_0> {
public:
    int timeout_ms() { return 0; }
    int verify_execution_time_ms() { return -1; }
    VerifyExecutionTime::report report_func() { return 0; }
};


class with_timeout_2_without_verify
{
public:
    typedef shared_state<with_timeout_2_without_verify> ptr;
};


template<>
class shared_state_traits<with_timeout_2_without_verify> {
public:
    int timeout_ms() { return 2; }
    int verify_execution_time_ms() { return -1; }
    VerifyExecutionTime::report report_func() { return 0; }
};


class with_timeout_1_and_verify_1 {};

template<>
class shared_state_traits<with_timeout_1_and_verify_1> {
public:
    int timeout_ms() { return 1; }
    int verify_execution_time_ms() { return 1; }
    VerifyExecutionTime::report report_func() { return 0; }
};


// Implements Callable
class WriteWhileReadingThread
{
public:
    WriteWhileReadingThread(B::ptr b);

    void operator()();

private:
    B::ptr b;

public:
    static void test();
};


void shared_state_test::
        test ()
{
    // It should guarantee compile-time thread safe access to objects.
    shared_state<A> mya {new A};

    // can't read from mya
    // error: passing 'volatile A' as 'this' argument of 'int A::a () const' discards qualifiers
    // mya->a ();

    // can't write to mya
    // error: passing 'volatile A' as 'this' argument of 'void A::a (int)' discards qualifiers
    // mya->a (5);

    {
        // Lock for write access
        auto w = mya.write ();
        w->method (5);
        A& b = *w;
        b.method (5);
        // Unlock on out-of-scope
    }

    // Lock for a single call
    mya.write()->method (5);

    {
        // Lock for read access
        auto r = mya.read ();
        EXCEPTION_ASSERT_EQUALS (r->const_method (), 5);
        const A& b = *r;
        EXCEPTION_ASSERT_EQUALS (b.const_method (), 5);

        // can't write to mya with the read_ptr from .read()
        // error: passing 'const A' as 'this' argument of 'void A::a (int)' discards qualifiers
        // r->a (5);

        // Unlock on out-of-scope
    }

    // Lock for a single call
    mya.read ()->const_method ();

    // Can call volatile methods
    mya->volatile_member_method ();

    // Create a reference to a const instance
    A::const_ptr consta (mya);

    // Can call volatile const methods from a const_ptr
    consta->consttest ();

    // Can get read-only access from a const_ptr.
    consta.read ()->const_method ();

    // Can get unsafe access without locks using a shared_mutable_state
    shared_state<A>::shared_mutable_state (mya)->method (1);

    // Can not get write access to a const pointer.
    // consta.write (); // error: no matching member function for call to 'write'

    // Conditional critical section, don't wait if the lock is not available
    if (auto w = mya.try_write ())
    {
        w->method (5);
    }

    {
        // Example of bad practice
        // Assume 'a ()' is const and doesn't have any other side effects.

        int sum = 0;
        sum += mya.read ()->const_method ();
        sum += mya.read ()->const_method ();

        // A common assumption here would be that const_method () returns the
        // same result twice. But this is NOT guaranteed. Another thread might
        // change 'mya' with a .write() between the two calls.
        //
        // In general, using multiple .read() is a smell that you're doing it
        // wrong.
    }

    {
        // Example of good practice

        auto r = mya.read (); // Lock the data you need before you start using it.
        int sum = 0;
        sum += r->const_method ();
        sum += r->const_method ();

        // Assuming 'const_method ()' doesn't have any other side effects
        // 'mya' is guaranteed to not change between the two calls to
        // 'r->const_method ()'.
    }

    // The differences in the bad and good practices illustrated above is
    // especially important for write() that might modify an object in
    // several steps where snapshots of intermediate steps would describe
    // inconsistent states. Using inconsistent states in general results in
    // undefined behaviour (i.e crashes, or worse).

    // However, long routines might be blocking for longer than preferred.

    {
        // Good practice for long reading routines
        // Limit the time you need the lock by copying the data to a local
        // instance before you start using it. This requires a user specified
        // copy that ignores any values of shared_state.

        const A mylocal_a = *mya.read ();
        int sum = 0;
        sum += mylocal_a.const_method ();
        sum += mylocal_a.const_method ();

        // As 'mylocal_a' is not even known of to any other threads this will
        // surely behave as expected.
    }

    {
        // Good practice for long writing routines
        // Assuming you only have one producer of data (with one or multiple
        // consumers). This requires user specified assignment that ignores
        // any values of shared_state.

        A mylocal_a = *mya.read (); // Produce a writable copy
        mylocal_a.method (5);
        *mya.write () = mylocal_a;

        // This will not be as easy with multiple producers as you need to
        // manage merging.
        //
        // The easiest solution for multiple producers is the version proposed
        // in the beginning were the write_ptr from .write() is kept throughout
        // the scope of the work.
    }


    // An explanation of inline locks, or one-line locks, or locks for a single call.
    //
    // One-line locks are kept until the complete statement has been executed.
    // The destructor of write_ptr releases the lock when the instance goes
    // out-of-scope. Because the scope in which write_ptr is created is a
    // statement, the lock is released after the entire statement has finished
    // executing.
    //
    // So this example would create a deadlock.
    //     int deadlock = mya->write ()->a () + mya->write ()->a ();
    //
    // The following statement may ALSO create a deadlock if another thread
    // requests a write() after the first read() has returned but before the
    // second read() (because read_ptr from the first read() doesn't go out of
    // scope and release its lock until the entire statement is finished):
    //     int potential_deadlock = mya->read ()->a () + mya->read ()->a ();
    //
    //
    // Rule of thumb; avoid locking more than one object at a time, and never
    // lock the same object more than once at a time.
    WriteWhileReadingThread::test ();

    // It should be accessible from various pointer types
    {
        const A::ptr mya1{new A};
        mya1.read ();
        // mya1.write (); // Can't write to a const shared_state

        A::const_ptr mya2{new A};
        mya2.read ();

        A::ptr{new A}.read ();
        A::ptr{new A}.write ();
    }

    // shared_state can be used in a sorted container.
    {
        map<A::ptr, int> mymap;
        mymap.find (A::ptr{});
    }


    // Verify the behaviour of a practice commonly frowned upon; throwing from constructors.
    //
    // It should be fine to throw from the constructor as long as allocated
    // resources are taken care of as usual in any other scope without the help
    // of the explicit destructor corresponding to the throwing constructor.
    {
        bool outer_destructor = false;
        bool inner_destructor = false;
        EXPECT_EXCEPTION(int, ThrowInConstructor d(&outer_destructor, &inner_destructor));
        EXCEPTION_ASSERT(inner_destructor);
        EXCEPTION_ASSERT(!outer_destructor);
    }


    // shared_state should cause an overhead of less than 1.5 microseconds in a
    // 'release' build when 'verify_execution_time_ms >= 0'.
    {
        int N = 10000;

        shared_state<C> c{new C};
        TRACE_PERF("shared_state should cause a low default overhead");
        for (int i=0; i<N; i++)
        {
            c.write ();
            c.read ();
        }
    }


    // shared_state should cause an overhead of less than 0.3 microseconds in a
    // 'release' build when 'verify_execution_time_ms < 0'.
    {
        int N = 10000;

        shared_ptr<A> a{new A};
        TRACE_PERF("shared_state should cause a low overhead : reference");
        for (int i=0; i<N; i++)
            a->noinlinecall ();

        A::ptr a2{new A};
        trace_perf_.reset("shared_state should cause a low overhead without verify_execution_time_ms");
        for (int i=0; i<N; i++)
        {
            a2.write ()->noinlinecall();
            a2.read ()->noinlinecall();
        }
    }


    // shared_state should cause an overhead of less than 0.1 microseconds in a
    // 'release' build when using 'no_lock_failed'.
    //
    // shared_state should fail fast when using 'no_lock_failed', within 0.1
    // microseconds in a 'release' build.
    {
        int N = 1000;

        with_timeout_0::ptr a{new with_timeout_0};
        with_timeout_0::const_ptr consta{a};

        // Make subsequent lock attempts fail
        with_timeout_0::ptr::write_ptr r = a.write ();

        TRACE_PERF("shared_state should fail fast with no_lock_failed");
        for (int i=0; i<N; i++) {
            a.try_write ();
            a.try_read ();
            consta.try_read ();
        }

        trace_perf_.reset("shared_state should fail fast with timeout=0");
        for (int i=0; i<N; i++) {
            EXPECT_EXCEPTION(lock_failed, a.write ());
        }
    }
}


A& A::
        operator= (const A& b)
{
    a_ = b.a_;
    c_ = b.c_;
    return *this;
}


void A::
        volatile_member_method () volatile
{
    // Can't call non-volatile methods
    // error: no matching member function for call to 'a'
    // this->a ();
    // (there is a method a () but it is not declared volatile)

    // volatile applies to member variables
    // error: no viable overloaded '='
    // this->c_ = 3;
    // (there is an operator= but it is not declared volatile)
}


void A::
        consttest () const volatile
{
    // Can't call non-volatile methods
    // error: no matching member function for call to 'a'
    // this->a ();
    // (there is a method a () but it is not declared const volatile)

    // const volatile applies to member variables
    // error: no viable overloaded '='
    // this->c_ = 3;
    // (there is an operator= but it is not declared const volatile)
}


int B::
    work_a_lot(int /*i*/) const
{
    this_thread::sleep_for (chrono::milliseconds{5});
    return 0;
}


WriteWhileReadingThread::
        WriteWhileReadingThread(B::ptr b)
    :
      b(b)
{}


void WriteWhileReadingThread::
        operator() ()
{
    // Make sure readTwice starts before this function
    this_thread::sleep_for (chrono::milliseconds{1});

    // Write access should fail as the first thread attempts recursive locks
    // through multiple calls to read ().
    EXPECT_EXCEPTION(lock_failed, b.write (); );
}


void readTwice(B::ptr b) {
    // int i = b.read()->work_a_lot(1) + b.read()->work_a_lot(2);
    // faster than default timeout
    int i = b.read ()->work_a_lot(3)
            + b.read ()->work_a_lot(4);
    (void) i;
}


void writeTwice(B::ptr b) {
    // int i = b.write()->work_a_lot(3) + b.write()->work_a_lot(4);
    // faster than default timeout
    int i = b.write ()->work_a_lot(1)
            + b.write ()->work_a_lot(2);
    (void) i;
}


// Implements Callable
class UseAandB
{
public:
    UseAandB(with_timeout_2_without_verify::ptr a, with_timeout_2_without_verify::ptr b) : a(a), b(b) {}

    void operator()() {
        try {

            auto aw = a.write ();
            this_thread::sleep_for (chrono::milliseconds{1});
            auto bw = b.write ();

        } catch (lock_failed& x) {
            const Backtrace* backtrace = boost::get_error_info<Backtrace::info>(x);
            EXCEPTION_ASSERT(backtrace);
        }
    }

private:
    with_timeout_2_without_verify::ptr a;
    with_timeout_2_without_verify::ptr b;

public:
    static void test();
};


void WriteWhileReadingThread::
        test()
{
    // It should detect deadlocks from recursive locks
    {
        B::ptr b{new B};

        // can't lock for write twice (recursive locks)
        EXPECT_EXCEPTION(B::ptr::lock_failed, writeTwice(b));
        EXPECT_EXCEPTION(lock_failed, writeTwice(b));

        // may be able to lock for read twice if no other thread locks for write in-between, but it is not guaranteed
        //readTwice(b);

        // can't lock for read twice if another thread request a write in the middle
        // that write request will fail but try_again will make this thread throw the error as well
        std::future<void> f = std::async(std::launch::async, WriteWhileReadingThread{b});
        EXPECT_EXCEPTION(lock_failed, readTwice(b));

        f.get ();
    }

    // It should produce run-time exceptions with backtraces on deadlocks
    {
        with_timeout_2_without_verify::ptr a{new with_timeout_2_without_verify};
        with_timeout_2_without_verify::ptr b{new with_timeout_2_without_verify};

        std::future<void> f = std::async(std::launch::async, UseAandB{a,b});

        try {

            auto bw = b.write ();
            this_thread::sleep_for (chrono::milliseconds{1});
            auto aw = a.write ();

        } catch (lock_failed& x) {
            const Backtrace* backtrace = boost::get_error_info<Backtrace::info>(x);
            EXCEPTION_ASSERT(backtrace);
        }

        f.get ();
    }

    // It should silently warn if a lock is kept so long that a simultaneous lock
    // attempt would fail.
    {
        shared_state<with_timeout_1_and_verify_1> a{new with_timeout_1_and_verify_1};

        bool did_report = false;

        VerifyExecutionTime::set_default_report ([&did_report](float, float){ did_report = true; });
        auto w = a.write ();
        VerifyExecutionTime::set_default_report (0);

        this_thread::sleep_for (chrono::milliseconds{10});

        EXCEPTION_ASSERT(!did_report);
        w.unlock ();
        EXCEPTION_ASSERT(did_report);
    }
}
