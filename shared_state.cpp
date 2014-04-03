/**
  This file only contains unit tests for shared_state.
  This file is not required for using shared_state.
  */

#include "shared_state.h"
#include "exceptionassert.h"
#include "expectexception.h"
#include "trace_perf.h"
#include "shared_state_traits_backtrace.h"
#include "barrier.h"

#include <thread>
#include <future>
#include <condition_variable>

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
    ThrowInConstructor(bool*outer_destructor, bool*inner_destructor)
        :
          outer_destructor(outer_destructor),
          inner(inner_destructor)
    {
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

    int     noinlinecall() const { cout << ""; return 0; }

private:

    int a_;
    OrderOfOperationsCheck c_;
};


template<>
struct shared_state_traits<A>: shared_state_traits_default {
    double timeout() { return 0.001; }
};


class B
{
public:
    struct shared_state_traits: shared_state_traits_default {
        double timeout() { return 0.010; }
    };

    typedef shared_state<B> ptr;

    int work_a_lot(int i) const;
};


struct C {
    void somework(int N) const {
        for (int i=0; i<N; i++)
            rand();
    }
};


struct C2: C {
    struct shared_state_traits: shared_state_traits_default {
        typedef shared_state_mutex_notimeout_noshared shared_state_mutex;

        double timeout() { return -1; }
    };
};


class with_timeout_0
{
public:
    typedef shared_state<with_timeout_0> ptr;
    typedef shared_state<const with_timeout_0> const_ptr;
};


template<>
struct shared_state_traits<with_timeout_0>: shared_state_traits_default {
    double timeout() { return 0; }
};


class with_timeout_1_and_verify_1 {};

template<>
struct shared_state_traits<with_timeout_1_and_verify_1>
: shared_state_traits_default
{
  double timeout() { return 0.001; }

  void was_locked() {
    start = chrono::high_resolution_clock::now ();
  }

  void was_unlocked() {
    chrono::duration<double> diff = chrono::high_resolution_clock::now () - start;
    if (diff.count() > verify_execution_time)
      exceeded_execution_time (diff.count());
  }

  float verify_execution_time = 0.001;
  function<void(double)> exceeded_execution_time = [](double T) {
    cout << "Warning: Lock of MyType was held for " << T << "seconds" << endl;
  };
private:
  chrono::high_resolution_clock::time_point start;
};


class base {
public:
    struct shared_state_traits: shared_state_traits_default {
        base* b;

        void was_locked () {
            EXCEPTION_ASSERT_EQUALS(++b->step, 1);
        }
        void was_unlocked () {
            ++b->step;
        }
    };

    virtual ~base() {}

    int step = 0;
};


class derivative: public base {
public:
    void method() {
        EXCEPTION_ASSERT_EQUALS(++step, 2);
    }
};


struct WriteWhileReadingThread
{
    static void test();
};


void shared_state_test::
        test ()
{
    // It should guarantee compile-time thread safe access to objects.
    shared_state<A> mya {new A};

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
    mya->method (5);

    {
        // Lock for read access
        auto r = mya.read ();
        EXCEPTION_ASSERT_EQUALS (r->const_method (), 5);
        const A& b = *r;
        EXCEPTION_ASSERT_EQUALS (b.const_method (), 5);

        // can't write to mya with the read_ptr from .read()
        // r->method (5);
        // error: member function 'method' not viable: 'this' argument has type 'const A', but function is not marked const

        // Unlock on out-of-scope
    }

    // Lock for a single call
    mya.read ()->const_method ();

    // Create a reference to a const instance
    shared_state<const A> consta {mya};

    // Can get read-only access from a const_ptr.
    consta.read ()->const_method ();
    consta->const_method (); // <- read lock
    mya->const_method ();    // <- write lock

    // Can get unprotected access without locks
    mya.raw ()->method (1);

    // Can not get write access to a const pointer.
    // consta.write (); // error

    // Can get a locked pointer
    mya.get ()->method (1);
    consta.get ()->const_method ();
    (*mya.write ()).method (1);

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


    // shared_state should cause an overhead of less than 0.3 microseconds in a
    // 'release' build.
    {
        int N = 10000;

        shared_ptr<A> a {new A};
        TRACE_PERF ("shared_state should cause a low overhead : reference");
        for (int i=0; i<N; i++)
            a->noinlinecall ();

        A::ptr a2 {new A};
        trace_perf_.reset ("shared_state should cause a low write overhead");
        for (int i=0; i<N; i++)
            a2.write ()->noinlinecall ();

        trace_perf_.reset ("shared_state should cause a low read overhead");
        for (int i=0; i<N; i++)
            a2.read ()->noinlinecall ();
    }


    // shared_state should cause an overhead of less than 0.1 microseconds in a
    // 'release' build when using 'no_lock_failed'.
    //
    // shared_state should fail fast when using 'no_lock_failed', within 0.1
    // microseconds in a 'release' build.
    {
        int N = 10000;
        with_timeout_0::ptr a{new with_timeout_0};
        with_timeout_0::const_ptr consta{a};

        // Make subsequent lock attempts fail
        with_timeout_0::ptr::write_ptr r = a.write ();

        TRACE_PERF ("shared_state should fail fast with try_write");
        for (int i=0; i<N; i++)
            a.try_write ();

        trace_perf_.reset ("shared_state should fail fast with try_read");
        for (int i=0; i<N; i++) {
            a.try_read ();
            consta.try_read ();
        }

        N = 1000;
        trace_perf_.reset ("shared_state should fail fast with timeout=0");
#ifndef SHARED_STATE_NO_TIMEOUT
        for (int i=0; i<N; i++) {
            EXPECT_EXCEPTION(lock_failed, a.write ());
        }
#endif
    }

    // It should keep the lock for the duration of a statement
    shared_state<base> b(new derivative);
    b.traits ()->b = b.raw ();
    dynamic_cast<derivative*>(b.write ().get ())->method ();
    EXCEPTION_ASSERT_EQUALS(b.raw ()->step, 3);

    condition_variable_any cond;
    future<void> f = async(launch::async, [&](){
        // Make sure readTwice starts before this function
        auto w = mya.write ();
        w->method (1);
        // wait unlocks w before waiting
        cond.wait(w);
        // w is locked again when wait return
        w->method (2);
    });

    while (future_status::timeout == f.wait_for(std::chrono::milliseconds(1)))
    {
        mya.write ()->method (3);
        cond.notify_one ();
    }

    auto rlock = mya.read ();
    auto rlock2 = std::move(rlock); // Move lock
    rlock2.swap (rlock); // Swap lock
    rlock.unlock ();

    auto wlock = mya.write ();
    auto wlock2 = std::move(wlock); // Move lock
    wlock2.swap (wlock); // Swap lock
    wlock.unlock ();
}


A& A::
        operator= (const A& b)
{
    a_ = b.a_;
    c_ = b.c_;
    return *this;
}


int B::
    work_a_lot(int /*i*/) const
{
    this_thread::sleep_for (chrono::milliseconds{5});
    return 0;
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


void WriteWhileReadingThread::
        test()
{
    // It should detect deadlocks from recursive locks
    {
        B::ptr b{new B};

#ifndef SHARED_STATE_NO_TIMEOUT
        // can't lock for write twice (recursive locks)
        EXPECT_EXCEPTION(B::ptr::lock_failed, writeTwice(b));
        EXPECT_EXCEPTION(lock_failed, writeTwice(b));
#endif

#ifndef SHARED_STATE_NO_SHARED_MUTEX
        // may be able to lock for read twice if no other thread locks for write in-between, but it is not guaranteed
        readTwice(b);
#endif

#ifndef SHARED_STATE_NO_TIMEOUT
        // can't lock for read twice if another thread request a write in the middle
        // that write request will fail and then this will succeed
        spinning_barrier barrier(2);
        future<void> f = async(launch::async, [&b,&barrier](){
            // Make sure readTwice starts before this function
            barrier.wait ();
            this_thread::sleep_for (chrono::milliseconds{3});

            // Write access should fail as the first thread attempts recursive locks
            // through multiple calls to read ().
            EXPECT_EXCEPTION(lock_failed, b.write (); );
        });

        {
            barrier.wait ();
#ifndef SHARED_STATE_NO_SHARED_MUTEX
            readTwice(b);
#else
            EXPECT_EXCEPTION(lock_failed, readTwice(b) );
#endif
        }

        f.get ();
#endif
    }


    // It should be extensible enough to let clients efficiently add features like
    //  - run-time warnings on locks that are kept long enough to make it likely
    //    that other simultaneous lock attempts will fail.
    {
        shared_state<with_timeout_1_and_verify_1> a{new with_timeout_1_and_verify_1};

        bool did_report = false;

        a.traits ()->exceeded_execution_time = [&did_report](float){ did_report = true; };

        auto w = a.write ();

        // Wait to make VerifyExecutionTime detect that the lock was kept too long
        this_thread::sleep_for (chrono::milliseconds{10});

        EXCEPTION_ASSERT(!did_report);
        w.unlock ();
        EXCEPTION_ASSERT(did_report);

        {
            int N = 10000;

            TRACE_PERF("shared_state with VerifyExecutionTime should cause a low overhead");
            for (int i=0; i<N; i++)
            {
                a.write ();
                a.read ();
            }
        }
    }

    // it should handle lock contention efficiently
    // 'Mv' decides how long a lock should be kept
    std::vector<int> Mv{100, 1000};
    for (unsigned l=0; l<Mv.size (); l++)
    {
        int M = Mv[l];
        int N = 200;

        // 'W' decides how often a worker should do a write instead of a read
        std::vector<int> W{1, 10, 100, 1000};

        for (unsigned k=0; k<W.size (); k++)
        {
            int w = W[k];
            TRACE_PERF ((boost::format("shared_state should handle lock contention efficiently N=%d, M=%d, w=%d") % N % M % w).str());

            vector<future<void>> workers(8);

            shared_state<C> c {new C};

            for (unsigned i=0; i<workers.size (); i++)
                workers[i] = async(launch::async, [&c,N,M,w](){
                    for(int j=1; j<=N; j++)
                        if (j%w)
                            c.read ()->somework (M);
                        else
                            c.write ()->somework (M);
                });

            for (unsigned i=0; i<workers.size (); i++)
                workers[i].get ();
        }

        {
            TRACE_PERF ((boost::format("shared_state should handle lock contention efficiently reference N=%d, M=%d") % N % M).str());

            vector<future<void>> workers(8);

            shared_ptr<C> c {new C};

            for (unsigned i=0; i<workers.size (); i++)
                workers[i] = async(launch::async, [&c,N,M](){
                    for(int j=1; j<=N; j++)
                        c->somework (M);
                });

            for (unsigned i=0; i<workers.size (); i++)
                workers[i].get ();
        }

        {
            TRACE_PERF ((boost::format("shared_state should handle lock contention efficiently simple N=%d, M=%d") % N % M).str());

            vector<future<void>> workers(8);

            // C2 doesn't have shared read-only access, so a read and a write lock are equivalent.
            shared_state<C2> c2 {new C2};

            for (unsigned i=0; i<workers.size (); i++)
                workers[i] = async(launch::async, [&c2,N,M](){
                    for(int j=1; j<=N; j++)
                        c2.write ()->somework (M);
                });

            for (unsigned i=0; i<workers.size (); i++)
                workers[i].get ();
        }
    }
}
