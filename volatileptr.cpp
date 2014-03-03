#include "volatileptr.h"
#include "exceptionassert.h"
#include "expectexception.h"

#include "timer.h"
#include "detectgdb.h"

#include <boost/thread/thread.hpp>
#include <boost/bind.hpp>

using namespace boost;

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


class A: public VolatilePtr<A>
{
public:
    A () {}
    A (const A& b) { *this = b; }
    A& operator= (const A& b);

    int     a () const { return a_; }
    void    a (int v) { a_ = v; }

    void    test () volatile;
    void    consttest () const volatile;

    void b() { }

    boost::shared_mutex* readWriteLock() const volatile {
        return VolatilePtr::readWriteLock ();
    }

private:

    int a_;
    OrderOfOperationsCheck c_;
};

class simple_semaphore : public VolatilePtr<simple_semaphore>
{
public:
    void aquire1() volatile { WritePtr(this)->aquire1(); }
    void release1() volatile { WritePtr(this)->release1(); }

private:
    void aquire1() {
        while (0 == i)
            condition.wait( *readWriteLock() );
        i--;
    }

    void release1() {
        i++;
        condition.notify_one();
    }

    int i;
    condition_variable_any condition;
};


class B: public VolatilePtr<B>
{
public:
    simple_semaphore semaphore;
};


// Implements Callable
class WriteWhileReadingThread
{
public:
    WriteWhileReadingThread(B::Ptr b);

    void operator()();

private:
    B::Ptr b;

public:
    static void test();
};

void VolatilePtrTest::
        test ()
{
    // It should guarantee compile-time thread safe access to objects.

    EXCEPTION_ASSERT_EQUALS (sizeof (VolatilePtr<A>), sizeof (boost::shared_mutex));
#ifndef _MSC_VER
    EXCEPTION_ASSERT_EQUALS (sizeof (boost::shared_mutex), 408u);
#else
    EXCEPTION_ASSERT_EQUALS (sizeof (boost::shared_mutex), sizeof(void*) * 4u);
#endif

    A::Ptr mya (new A);

    // can't write to mya
    // error: passing 'volatile A' as 'this' argument of 'void A::a (int)' discards qualifiers
    // mya->a (5);

    {
        // Lock for write access
        A::WritePtr w (mya);
        w->a (5);
        A& b = *w;
        b.a (5);
        // Unlock on out-of-scope
    }

    // Lock for a single call
    A::WritePtr (mya)->a (5);
    write1(mya)->a (5);

    // can't read from mya
    // error: passing 'volatile A' as 'this' argument of 'int A::a () const' discards qualifiers
    // mya->a ();

    {
        // Lock for read access
        A::ReadPtr r (mya);
        EXCEPTION_ASSERT_EQUALS (r->a (), 5);
        const A& b = *r;
        EXCEPTION_ASSERT_EQUALS (b.a (), 5);

        // can't write to mya with ReadPtr
        // error: passing 'const A' as 'this' argument of 'void A::a (int)' discards qualifiers
        // r->a (5);

        // Unlock on out-of-scope
    }

    // Lock for a single call
    A::ReadPtr (mya)->a ();
    read1(mya)->a ();

    // Can call volatile methods
    mya->test ();

    // Create a volatile reference to a const instance
    A::ConstPtr consta (mya);

    // Can call volatile const methods from a ConstPtr
    consta->consttest ();

    // Can get read-only access from a ConstPtr.
    A::ReadPtr (consta)->a ();

    // Can not create a WritePtr to a const pointer.
    // error: no matching function for call to 'VolatilePtr<A>::WritePtr::WritePtr (VolatilePtr<A>::ConstPtr)'
    // A::WritePtr (consta)->a ();

    // Locked ptr can not call volatile methods because recursive locking is
    // not allowed. This results in a deadlock but VolatilePtr throws a
    // LockFailed if the lock couldn't be obatined within a timeout.
    EXPECT_EXCEPTION(LockFailed, A::WritePtr (mya, 1)->test ());

    {
        // Bad practice
        // Assume 'a ()' is const and doesn't have any other side effects.
        int sum = 0;
        sum += read1 (mya)->a ();
        sum += read1 (mya)->a ();
        // A common assumption here would be that a () returns the same result
        // twice. But this is NOT guaranteed. Another thread might change 'mya'
        // with a WritePtr between the two calls.
        //
        // That is also how methods with synchronized behave in Java.
        //
        // In general, using multiple read1 is a smell that you're doing it wrong.
    }

    {
        // Good practice
        A::ReadPtr r (mya); // Lock the data you need before you start using it.
        int sum = 0;
        sum += r->a ();
        sum += r->a ();
        // Assuming 'a ()' is const and doesn't have any other side effects
        // 'mya' is guaranteed to not change between the two calls to r->a ().
    }

    // The differences in the bad and good practices illustrated above is
    // especially important for WritePtr that might modify an object in
    // several steps where snapshots of intermediate steps would describe
    // inconsistent states. Using inconsistent states most often results in
    // undefined behaviour (i.e crashes, or worse).
    //
    // However, long routines might be blocking for longer than preferred.

    {
        // Good practice for long reading routines
        // Limit the time you need the lock by copying the data to a local
        // instance before you start using it. This requires a user specified
        // copy that ignores any values of VolatilePtr.
        const A mylocal_a = *A::ReadPtr (mya);
        int sum = 0;
        sum += mylocal_a.a ();
        sum += mylocal_a.a ();
        // As 'mylocal_a' is not even known of to any other threads this will
        // surely behave as expected.
    }

    {
        // Good practice for long writing routines
        // Assuming you only have one producer of data (with one or multiple
        // consumers). This requires user specified assignment that ignores
        // any values of VolatilePtr.
        A mylocal_a = *A::ReadPtr (mya); // Produce a writable copy
        mylocal_a.a (5);
        *A::WritePtr (mya) = mylocal_a;
        // This will not be as easy with multiple producers as you need to
        // manage merging afterwards.
        //
        // The easiest solution for multiple producers is the
        // version proposed at the top were the WritePtr is kept throughout the
        // scope of the work.
    }


    // An explanation of inline locks, or one-line locks, or locks for a single call.
    //
    // One-line locks are kept until the complete statement has been executed.
    // The destructor of A::WritePtr releases the lock when the instance goes
    // out-of-scope. Because the scope in which A::WritePtr is created is a
    // statement, the lock is released after the entire statement has finished
    // executing.
    //
    // So this example would create a deadlock. When that happens, attach a
    // debugger to see on which line this occured.
    // int deadlock = A::WritePtr (mya)->a () + A::WritePtr (mya)->a ();
    //
    // The following statement will ALSO create a deadlock if another thread
    // requests an A::WritePtr after the first ReadPtr is obtained but before
    // the second ReadPtr is obtained (because the first ReadPtr is not
    // released until the entire statement is finished):
    // int deadlock = A::ReadPtr (mya)->a () + A::ReadPtr (mya)->a ();
    //
    // Rule of thumb; avoid locking more than one object at a time, and avoid
    // locking the same object more than once at a time.
    WriteWhileReadingThread::test ();

    // It should be accessible from various pointer types
    {
        const A::Ptr mya1(new A);
        {A::ReadPtr r(mya1);}
        {A::WritePtr w(mya1);}

        const volatile A::Ptr mya2(new A);
        {A::ReadPtr r(mya2);}
        {A::WritePtr w(mya2);}

        const volatile A::ConstPtr mya3(new A);
        {A::ReadPtr r(mya3);}

        A::ConstPtr mya4(new A);
        A::ReadPtr(mya4.get ());

        A::WritePtr(mya.get ());
    }

    // It should be fine to throw from the constructor as long as allocated resources
    // are taken care of as usual in any other scope.
    {
        bool outer_destructor = false;
        bool inner_destructor = false;
        EXPECT_EXCEPTION(int, ThrowInConstructor d(&outer_destructor, &inner_destructor));
        EXCEPTION_ASSERT(inner_destructor);
        EXCEPTION_ASSERT(!outer_destructor);
    }

    // More thoughts on design decisions
    // VolatilePtr provides two short methods read1 and write1. They are a bit
    // controversial as they are likely to lead to inconsistent coding styles
    // when mixing the two versions.
    //  sum += read1 (mya)->a ()
    //  sum += A::ReadPtr (mya)->a ()
    // The latter is more explicit which in this case is good to emphasize that
    // a wrapper object is being constructed to manage access. It's also
    // generic enough to be used as the only way to lock an object.
    //
    // read1 and write1 are bad smells.
}


A& A::
        operator= (const A& b)
{
    a_ = b.a_;
    c_ = b.c_;
    return *this;
}


void A::
        test () volatile
{
    // Lock on 'this'. A suggested practice is to refer to the non-volatile this as 'self'.
    A::WritePtr self (this, 1);
    self->a (5);
    self->c_ = 3;
}


void A::
        consttest () const volatile
{
    // Lock on 'this'

    // Note. To perform a lock ReadPtr needs non-const access to
    // this->VolatilePtr<A>::lock_. It would make sense though to create a
    // ReadPtr from a const volatile instance. But this is a limited well
    // defined access that doesn't violate the semantic meaning of "const A".
    // So we let ReadPtr cast away the constness to perform the lock. ReadPtr
    // then only provides a const reference through its accessors.
    A::ReadPtr (this)->a ();
}


WriteWhileReadingThread::
        WriteWhileReadingThread(B::Ptr b)
    :
      b(b)
{}

#include "tasktimer.h"

void WriteWhileReadingThread::
        operator() ()
{
    b->semaphore.release1();

    // Write access should succeed when the first thread throws LockFailed.
    B::WritePtr wp(b);
}

void readTwice(B::Ptr b) {
    B::ReadPtr rp(b,10);

    b->semaphore.aquire1(); // wait for the thread to start

    // Assume this sleep is enough for the other thread to try to lock B with
    // B::WritePtr. This implementation is timing-dependent and its purpose is
    // to highlight a problem that is timing-dependent (i.e the fact that
    // obtaining two instances of ReadPtr might succeed but its not
    // guaranteed).
    this_thread::sleep_for (chrono::milliseconds(1));
    B::ReadPtr rp2(b,10);
}

void writeTwice(B::Ptr b) {
    B::WritePtr wp(b,10);
    B::WritePtr wp2(b,10);
}

void WriteWhileReadingThread::
        test()
{
    bool debug = false;
#ifdef _DEBUG
    debug = true;
#endif

    bool win32 = false;
#ifdef _MSC_VER
    if (sizeof(void*)==4)
        win32 = true;
#endif
    // It should cause a dead-lock and detect it as such
    {
        Timer timer;
        B::Ptr b(new B);

        // can't lock for write twice
        EXPECT_EXCEPTION(B::LockFailed, writeTwice(b));
        EXPECT_EXCEPTION(LockFailed, writeTwice(b));

        // can't lock for read twice if another thread request a write in the middle
        WriteWhileReadingThread wb(b);
        boost::thread t = boost::thread(wb);
        // can't lock for read twice if another thread locks for write inbetween
        EXPECT_EXCEPTION(LockFailed, readTwice(b));
        t.join ();

        double T = timer.elapsed ();
        EXCEPTION_ASSERT_LESS(40e-3, T);
        EXCEPTION_ASSERT_LESS(T, 160e-3);
    }

    // 'NoLockFailed' should be fast
    {
        A::Ptr a(new A());
        A::ConstPtr consta(a);

        A::WritePtr r(a);
        double T;

        bool gdb = DetectGdb::is_running_through_gdb();

        Timer timer;
        for (int i=0; i<1000; i++) {
            a->readWriteLock ()->try_lock ();
        }
        T = timer.elapsedAndRestart ()/1000;
        EXCEPTION_ASSERT_LESS(T, debug ? gdb ? 10000e-9 : 250e-9 : 30e-9);
#ifdef _MSC_VER
        EXCEPTION_ASSERT_LESS(debug ? 20e-9 : 2e-9, T);
#else
        EXCEPTION_ASSERT_LESS(debug ? 20e-9 : 20e-9, T);
#endif

        for (int i=0; i<1000; i++) {
            a->readWriteLock ()->try_lock_for(boost::chrono::milliseconds(0));
        }
        T = timer.elapsedAndRestart ()/1000;
        EXCEPTION_ASSERT_LESS(T, 4000e-9);
#ifdef _MSC_VER
        EXCEPTION_ASSERT_LESS(200e-9, T);
#else
        EXCEPTION_ASSERT_LESS(300e-9, T);
#endif

        int failed_locks_count = 1000;
#ifdef _MSC_VER
        // Creating backtraces on windows is slow as symbols are resolved right away
        failed_locks_count = debug ? 1 : 2;
#endif

        for (int i=0; i<failed_locks_count; i++) {
            try { A::WritePtr(a,0); } catch (const LockFailed&) {}
        }
        T = timer.elapsedAndRestart ()/1000;
        EXCEPTION_ASSERT_LESS(T, debug ? 40000e-9 : 30000e-9);
        EXCEPTION_ASSERT_LESS(debug ? 9000e-9 : 6000e-9, T);

        for (int i=0; i<failed_locks_count; i++) {
            EXPECT_EXCEPTION(LockFailed, A::WritePtr(a,0));
        }
        T = timer.elapsedAndRestart ()/1000;
        EXCEPTION_ASSERT_LESS(T, debug ? gdb ? 40000e-9 : 28000e-9 : 28000e-9);
        EXCEPTION_ASSERT_LESS(debug ? 9000e-9 : 6000e-9, T);

        for (int i=0; i<1000; i++) {
            A::WritePtr(a,NoLockFailed());
        }
        T = timer.elapsedAndRestart ()/1000;
#ifdef _MSC_VER
        EXCEPTION_ASSERT_LESS(T, debug ? 130e9 : 30e-9);
        EXCEPTION_ASSERT_LESS(debug ? 100e-9 : 13e-9, T);
#else
        EXCEPTION_ASSERT_LESS(T, debug ? gdb ? 270e-9 : 220e-9 : 210e-9);
        EXCEPTION_ASSERT_LESS(debug ? 50e-9 : 33e-9, T);
#endif

        for (int i=0; i<1000; i++) {
            A::ReadPtr(a,NoLockFailed());
        }
        T = timer.elapsedAndRestart ()/1000;
#ifdef _MSC_VER
        EXCEPTION_ASSERT_LESS(T, debug ? 700e-9 : 30e-9);
        EXCEPTION_ASSERT_LESS(debug ? 100e-9 : 20e-9, T);
#else
        EXCEPTION_ASSERT_LESS(T, debug ? gdb ? 250e-9 : 170e-9 : 190e-9);
        EXCEPTION_ASSERT_LESS(debug ? 50e-9 : 32e-9, T);
#endif

        for (int i=0; i<1000; i++) {
            A::ReadPtr(consta,NoLockFailed());
        }
        T = timer.elapsedAndRestart ()/1000;
#ifdef _MSC_VER
        EXCEPTION_ASSERT_LESS(T, debug ? win32 ? 600e-9 : 180e-9 : 40e-9);
        EXCEPTION_ASSERT_LESS(debug ? 100e-9 : 15e-9, T);
#else
        EXCEPTION_ASSERT_LESS(T, debug ? gdb ? 200e-9 : 260e-9 : 190e-9);
        EXCEPTION_ASSERT_LESS(debug ? 50e-9 : 32e-9, T);
#endif
    }

    // It should cause a low overhead
    {
        bool debug = false;
    #ifdef _DEBUG
        debug = true;
    #endif

        int N = 100000;
        boost::shared_ptr<A> a(new A());
        Timer timer;
        for (int i=0; i<N; i++)
            a->a();
        double T = timer.elapsed ()/N;

        A::Ptr a2(new A());
        Timer timer2;
        for (int i=0; i<N; i++)
            A::WritePtr(a2)->a();
        double T2 = timer2.elapsed ()/N;

        Timer timer3;
        for (int i=0; i<N; i++)
            A::ReadPtr(a2)->a();
        double T3 = timer3.elapsed ()/N;

#ifdef __GCC__
        EXCEPTION_ASSERT_LESS(T2-T, 0.11e-6);
        EXCEPTION_ASSERT_LESS(T3-T, 0.11e-6);
#else
//        EXCEPTION_ASSERT_LESS(T2-T, 0.18e-6);
//        EXCEPTION_ASSERT_LESS(T3-T, 0.14e-6);
        EXCEPTION_ASSERT_LESS(T2-T, debug ? win32 ? 3e-6 : 0.7e-6 : 0.8e-6);
        EXCEPTION_ASSERT_LESS(T3-T, debug ? win32 ? 3e-6 : 0.7e-6 : 0.9e-6);
#endif
    }
}
