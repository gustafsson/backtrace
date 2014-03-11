/**
 * The shared_state class should guarantee thread-safe access to objects.
 * See details on the class shared_state below.
 *
 * Author: johan.b.gustafsson@gmail.com
 */

#ifndef SHARED_STATE_H
#define SHARED_STATE_H

#include "verifyexecutiontime.h"

#include <boost/thread/shared_mutex.hpp>
#include <boost/exception/all.hpp>

#ifndef SHARED_STATE_THROW_EXCEPTION
  #ifdef SHARED_STATE_NO_BACKTRACE
    #define SHARED_STATE_THROW_EXCEPTION(x) BOOST_THROW_EXCEPTION(x)
  #else
    #include "backtrace.h"
    #define SHARED_STATE_THROW_EXCEPTION(x) BOOST_THROW_EXCEPTION(x << Backtrace::make (2))
  #endif
#endif

class NoLockFailed {};
class LockFailed: public virtual boost::exception, public virtual std::exception {
public:
    typedef boost::error_info<struct timeout, int> timeout_value;

    /**
     When a timeout occurs on a lock shared_state makes an attempt to detect
     deadlocks. The thread with the timeout is blocked with another lock
     attempt long enough (same timeout as in the first attempt) for any other
     thread that is deadlocking with this thread to also fail its lock attempt.

     The try_again value sais whether that second lock attempt succeeded, but
     even if it succeeds LockFailed is still thrown.
     */
    typedef boost::error_info<struct try_again, bool> try_again_value;
};


#ifdef _DEBUG
// disable timeouts during debug sessions
//#define shared_state_lock_timeout_ms -1
#define shared_state_lock_timeout_ms 100
#else
#define shared_state_lock_timeout_ms 100
#endif


template<class C>
struct shared_state_traits {
    int timeout_ms() { return shared_state_lock_timeout_ms; }
    int verify_execution_time_ms() { return shared_state_lock_timeout_ms/2; }
    VerifyExecutionTime::report report_func() { return 0; }
};

template<typename T>
struct has_shared_state_traits
{
    // detect C::shared_state_traits::timeout_ms, assume
    // verify_execution_time_ms and report_func are also present
    template<typename C>
    static char test(decltype(&C::shared_state_traits::timeout_ms));

    template<typename C> // worst match
    static char (&test(...))[2];

    static const bool value = (sizeof( test<T>(0)  ) == 1);
};

template<class C, bool a = has_shared_state_traits<C>::value>
struct shared_state_traits_helper;

template<class C>
struct shared_state_traits_helper<C,false> {
    typedef shared_state_traits<C> type;
};

template<class C>
struct shared_state_traits_helper<C,true> {
    typedef typename C::shared_state_traits type;
};

class shared_state_details {
    template<typename Y> friend class shared_state;

    template<typename Traits>
    shared_state_details(Traits traits)
        : timeout_ms(traits.timeout_ms()),
          verify_execution_time_ms(traits.verify_execution_time_ms()),
          report_func(traits.report_func())
    {}

    shared_state_details(shared_state_details const&) = delete;
    shared_state_details& operator=(shared_state_details const&) = delete;

    mutable boost::shared_mutex lock;
    const int timeout_ms;
    const int verify_execution_time_ms;
    const VerifyExecutionTime::report report_func;
};


/**
 * The shared_state class should guarantee thread-safe access to objects, with
 *  - compile-time errors on missing locks
 *  - run-time exceptions with backtraces on deadlocks and failed locks
 *  - run-time warnings on locks that are kept long enough to make it likely
 *    that other simultaneous lock attempts will fail.
 *
 * Quick cheat sheet:
 *    class A {
 *    public:
 *       void foo();
 *       void bar() const;
 *       void baz() volatile;
 *    };
 *
 *    shared_state<A> a(new A); // Smart pointer that makes its data safe to use
 *                              // in multiple threads.
 *    a.write()->foo();         // Mutally exclusive write access
 *    a.read()->bar();          // Simultaneous read-only access
 *    a->baz();                 // use the volailte qualifier to denote
 *                              // thread-safe method
 *
 *    a.read()->foo(); <-- error  // a.read() gives only const access
 *    a->foo();        <-- error  // un-safe method call fails in compile time
 *
 *    try {
 *       a.write()->foo();
 *    } catch(LockFailed) {
 *       // Catch if a lock couldn't be obtained within a given timeout.
 *       // The timeout is set when instanciating shared_state<A>.
 *    }
 *
 *    shared_state<A>::write_ptr w(a, NoLockFailed()); // Returns immediately without waiting.
 *    if (w) w->foo();  // Only lock for access if readily availaible (i.e
 *                      // currently not locked by any other thread)
 *
 * For more complete examples see:
 *    shared_stateTest::test ()
 *
 *
 * To use shared_state to ensure thread-safe access to some previously
 * un-protected data of class MyType, wrap all 'new MyType' calls as arguments
 * to the constructor of shared_state<MyType>. Like in the example above for
 * 'class A'.
 *
 * The helper classes read_ptr and write_ptr then provides both thread-safe
 * access (by aquiring a lock) and exception-safe access (by RAII).
 *
 * The idea is let the 'volatile' qualifier denote that an object can be
 * modified by any thread at any time and use a pointer to a volatile object
 * when juggling references to objects. When you need the data you access it
 * safely through shared_state::read_ptr and shared_state::write_ptr.
 *
 * From a volatile object you can only access methods that are volatile (just
 * like only const methods are accessible from a const object). Using the
 * volatile classifier prevents access to use any "regular" (non-volatile)
 * methods.
 *
 * "The volatile keyword in C++11 ISO Standard code is to be used only for
 * hardware access; do not use it for inter-thread communication." [msdn]
 * shared_state doesn't use the volatile qualifier for inter-thread
 * communication but to ensure un-safe methods are called without adequate
 * locks.
 *
 * Idea based on this article:
 * http://www.drdobbs.com/cpp/volatile-the-multithreaded-programmers-b/184403766
 *
 *
 * shared_state should cause an overhead of less than 0.1 microseconds in a
 * 'release' build when using 'NoLockFailed'.
 *
 * shared_state should fail fast when using 'NoLockFailed', within 0.1
 * microseconds in a 'release' build.
 *
 * shared_state should cause an overhead of less than 0.3 microseconds in a
 * 'release' build when 'verify_execution_time_ms < 0'.
 *
 * shared_state should cause an overhead of less than 1.5 microseconds in a
 * 'release' build when 'verify_execution_time_ms >= 0'.
 *
 *
 * It is possible to use different timeouts, different expected execution times
 * and to disable the timeout and expected execution time. Create a template
 * specialization of shared_state_traits to override the defaults.
 *
 * Author: johan.b.gustafsson@gmail.com
 */
template<typename T>
class shared_state final
{
private:
    typedef const shared_state_details details;

public:
    typedef T element_type;
    typedef boost::shared_mutex shared_mutex;

    class LockFailed: public ::LockFailed {};

    shared_state () {}

    /**
     * If 'timeout_ms >= 0' read_ptr/write_ptr will try to look until the timeout
     * has passed and then throw a LockFailed exception. If 'timeout_ms < 0'
     * they will block indefinitely until the lock becomes available.
     *
     * If 'verify_execution_time_ms >= 0' read_ptr/WritePTr will call
     * report_func if the lock is kept longer than this value. With the default
     * behaviour of VerifyExecutionTime if report_func = 0.
     */
    template<class Y,
             class = typename std::enable_if
                     <
                        std::is_convertible<Y*, element_type*>::value
                     >::type
            >
    explicit shared_state ( Y* p )
    {
        reset(p);
    }

    template<class Y,
             class = typename std::enable_if
                     <
                        std::is_convertible<Y*, element_type*>::value
                     >::type
            >
    shared_state(const shared_state<Y>& a)
        :
            p( a.p ),
            d( a.d )
    {
    }

    template<class Y,
             class = typename std::enable_if
                     <
                        std::is_convertible<Y*, element_type*>::value
                     >::type
            >
    void reset( Y* yp=0 ) {
        if (yp)
        {
            p.reset (yp);
            d.reset (new details(typename shared_state_traits_helper< Y >::type()));
        }
        else
        {
            p.reset ();
            d.reset ();
        }
    }

    volatile T* operator-> () const { return p.get (); }
    volatile T& operator* () const { return *p.get (); }
    volatile T* get () const { return p.get (); }
    explicit operator bool() const { return p.get (); }
    bool operator== (const shared_state& b) const { return p.get () == b.p.get (); }
    bool operator!= (const shared_state& b) const { return !(*this == b); }
    bool operator < (const shared_state& b) const { return this->p < b.p; }

    class weak_ptr {
    public:
        weak_ptr() {}
        weak_ptr(const shared_state& t) : data(t.d), p(t.p) {}

        shared_state lock() const {
            std::shared_ptr<details> datap = data.lock ();
            std::shared_ptr<volatile T> pp = p.lock ();

            if (pp && datap)
                return shared_state(pp, datap);

            return shared_state(std::shared_ptr<T>(), std::shared_ptr<details>());
        }

    private:
        std::weak_ptr<details> data;
        std::weak_ptr<volatile T> p;
    };

    /**
     * @brief The shared_mutable_state class provides direct access to the
     * unprotected state. Consider using read_ptr or write_ptr instead.
     */
    class shared_mutable_state {
    public:
        shared_mutable_state() {}
        shared_mutable_state(const shared_state& t)
            :
              p(t.p),
              px(const_cast<const T*> (p.get ()))
        {}

        T* operator-> () const { return px; }
        T& operator* () const { return *px; }
        T* get () const { return px; }

    private:
        T* px;
        std::shared_ptr<volatile T> p;
    };

    /**
     * For examples of usage see void shared_stateTest::test ().
     *
     * The purpose of read_ptr is to provide thread-safe access to an a const
     * object for a thread during the lifetime of the read_ptr. This access
     * may be shared by multiple threads that simultaneously use their own
     * read_ptr to access the same object.
     *
     * The accessors without NoLockFailed always returns an accessible
     * instance, never null. If a lock fails a LockFailed exception is thrown.
     *
     * @see void shared_stateTest::test ()
     * @see class shared_state
     * @see class write_ptr
     */
    class read_ptr {
    public:
        template<class Y,
                 class = typename std::enable_if
                         <
                            std::is_convertible<Y*, const element_type*>::value
                         >::type
                >
        explicit read_ptr (const shared_state<Y>& vp)
            :   l (vp.readWriteLock()),
                p (vp.p),
                d (vp.d),
                px (const_cast<const T*> (vp.get ()))
        {
            lock ();
        }

        /**
         * @brief read_ptr with NoLockFailed obtains the lock if it is readily
         * available. If the lock was not obtained it doesn't throw any
         * exception, but the accessors returns a null pointer. This function
         * fails much faster than setting timeout_ms=0 and discarding any
         * LockFailed.
         */
        template<class Y,
                 class = typename std::enable_if
                         <
                            std::is_convertible<Y*, const element_type*>::value
                         >::type
                >
        explicit read_ptr (const shared_state<Y>& vp, NoLockFailed)
            :   l (vp.readWriteLock()),
                p (vp.p),
                d (vp.d),
                px (const_cast<const T*> (p.get ()))
        {
            if (!l.try_lock_shared ())
                px = 0;
        }

        read_ptr(read_ptr&& b)
            :   l (b.l),
                px (b.px)
        {
            p.swap (b.p);
            d.swap (b.d);
            b.px = 0;
        }

        read_ptr(const read_ptr&) = delete;
        read_ptr& operator=(read_ptr const&) = delete;

        ~read_ptr() {
            // The destructor isn't called if the constructor throws.
            unlock ();
        }

        const T* operator-> () const { return px; }
        const T& operator* () const { return *px; }
        const T* get () const { return px; }

        void unlock() {
            if (px)
            {
                l.unlock_shared ();
                pc.reset ();
            }

            px = 0;
        }

    private:
        void lock() {
            int timeout_ms = d->timeout_ms; // l is not locked, but timeout_ms is const

            // try_lock_shared_for and lock_shared are unnecessarily complex if
            // the lock is available right away
            if (l.try_lock_shared ())
            {
                // Got lock
            }
            else if (timeout_ms < 0)
            {
                l.lock_shared ();
                // Got lock
            }
            else if (l.try_lock_shared_for (boost::chrono::milliseconds(timeout_ms)))
            {
                // Got lock
            }
            else
            {
                // If this is a deadlock, make both threads throw by keeping this thread blocked.
                // See LockFailed::try_again
                bool try_again = l.try_lock_shared_for (boost::chrono::milliseconds(timeout_ms));
                if (try_again)
                    l.unlock_shared ();

                SHARED_STATE_THROW_EXCEPTION(LockFailed()
                                      << typename LockFailed::timeout_value(timeout_ms)
                                      << typename LockFailed::try_again_value(try_again));
            }

            if (0<d->verify_execution_time_ms)
                pc = VerifyExecutionTime::start (d->verify_execution_time_ms * 1e-3f, d->report_func);
        }

        shared_mutex& l;
        // p is 'const volatile T', compared to shared_state::p which is just 'volatile T'.
        std::shared_ptr<const volatile T> p;
        std::shared_ptr<details> d;
        const T* px;
        VerifyExecutionTime::Ptr pc;
    };


    /**
     * For examples of usage see void shared_stateTest::test ().
     *
     * The purpose of write_ptr is to provide exclusive access to an object for
     * a single thread during the lifetime of the write_ptr.
     *
     * @see class read_ptr
     */
    class write_ptr {
    public:
        template<class Y,
                 class = typename std::enable_if
                         <
                            std::is_convertible<Y*, element_type*>::value
                         >::type
                >
        explicit write_ptr (shared_state<Y> vp)
            :   l (vp.readWriteLock()),
                p (vp.p),
                d (vp.d),
                px (const_cast<T*> (p.get ()))
        {
            lock ();
        }

        // See read_ptr(const read_ptr&, NoLockFailed)
        template<class Y,
                 class = typename std::enable_if
                         <
                            std::is_convertible<Y*, element_type*>::value
                         >::type
                >
        explicit write_ptr (shared_state<Y> vp, NoLockFailed)
            :   l (vp.readWriteLock()),
                p (vp.p),
                d (vp.d),
                px (const_cast<T*> (p.get ()))
        {
            if (!l.try_lock ())
                px = 0;
        }

        write_ptr(write_ptr&& b)
            :   l (b.l),
                px (b.px)
        {
            p.swap (b.p);
            d.swap (b.d);
            b.px = 0;
        }

        write_ptr(const write_ptr&) = delete;
        write_ptr& operator=(write_ptr const&) = delete;

        ~write_ptr() {
            unlock ();
        }

        T* operator-> () const { return px; }
        T& operator* () const { return *px; }
        T* get () const { return px; }
        shared_state getPtr () const { return p; }

        void unlock() {
            if (px)
            {
                l.unlock ();
                pc.reset ();
            }

            px = 0;
        }

    private:
        // See read_ptr::lock
        void lock() {
            int timeout_ms = d->timeout_ms;

            if (l.try_lock())
            {
            }
            else if (timeout_ms < 0)
            {
                l.lock ();
            }
            else if (l.try_lock_for (boost::chrono::milliseconds(timeout_ms)))
            {
            }
            else
            {
                bool try_again = l.try_lock_for (boost::chrono::milliseconds(timeout_ms));
                if (try_again)
                    l.unlock ();

                SHARED_STATE_THROW_EXCEPTION(LockFailed()
                                      << typename LockFailed::timeout_value(timeout_ms)
                                      << typename LockFailed::try_again_value(try_again));
            }

            if (0<d->verify_execution_time_ms)
                pc = VerifyExecutionTime::start (d->verify_execution_time_ms * 1e-3f, d->report_func);
        }

        shared_mutex& l;
        std::shared_ptr<volatile T> p;
        std::shared_ptr<details> d;
        T* px;
        VerifyExecutionTime::Ptr pc;
    };


    read_ptr read() const volatile { return read_ptr(*const_cast<const shared_state*>(this)); }
    write_ptr write() volatile { return write_ptr(*const_cast<shared_state*>(this)); }

    /**
     * @brief readWriteLock returns the shared_mutex object for this instance.
     */
    shared_mutex& readWriteLock() const volatile { return (*const_cast<std::shared_ptr<details>*>(&d))->lock; }

private:
    template<typename Y>
    friend class shared_state;

    explicit shared_state ( std::shared_ptr<volatile T> p, std::shared_ptr<details> d ) : p(p), d(d) {}

    std::shared_ptr<volatile T> p;
    std::shared_ptr<details> d;
};

class shared_state_test {
public:
    static void test ();
};

#endif // SHARED_STATE_H
