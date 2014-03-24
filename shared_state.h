/**
 * The shared_state class should guarantee thread-safe access to objects.
 * See details on the class shared_state below.
 *
 * Author: johan.b.gustafsson@gmail.com
 */

#ifndef SHARED_STATE_H
#define SHARED_STATE_H

#include "verifyexecutiontime.h"

#include <boost/exception/exception.hpp>


#ifndef SHARED_STATE_THROW_EXCEPTION
  #ifdef SHARED_STATE_NO_BACKTRACE
    #define SHARED_STATE_THROW_EXCEPTION(x) BOOST_THROW_EXCEPTION(x)
  #else
    #include "backtrace.h"
    #define SHARED_STATE_THROW_EXCEPTION(x) BOOST_THROW_EXCEPTION(x << Backtrace::make (2))
  #endif
#endif


#ifdef SHARED_STATE_BOOST_MUTEX
    #include <boost/thread/shared_mutex.hpp>

    namespace shared_state_chrono = boost::chrono;

    #if defined SHARED_STATE_NO_TIMEOUT
        #if defined SHARED_STATE_NO_SHARED_MUTEX
            class shared_state_mutex: public boost::mutex {
            public:
                // recursive read locks are allowed to dead-lock so it is valid to replace a shared_timed_mutex with a mutex
                void lock_shared() { lock(); }
                bool try_lock_shared() { return try_lock(); }
                void unlock_shared() { unlock(); }

                // Discard any timeout parameters
                bool try_lock_for(...) { return try_lock(); }
                bool try_lock_shared_for(...) { return try_lock(); }
            };
        #else
            class shared_state_mutex: public boost::shared_mutex {
            public:
                // Discard any timeout parameters
                bool try_lock_for(...) { return try_lock(); }
                bool try_lock_shared_for(...) { return try_lock_shared(); }
            };
        #endif
    #elif defined SHARED_STATE_NO_SHARED_MUTEX
        class shared_state_mutex: public boost::timed_mutex {
        public:
            void lock_shared() { lock(); }
            bool try_lock_shared() { return try_lock(); }
            void unlock_shared() { unlock(); }

            template <class Rep, class Period>
            bool try_lock_shared_for(const shared_state_chrono::duration<Rep, Period>& rel_time) { return try_lock_for(rel_time); }
        };
    #else
        typedef boost::shared_mutex shared_state_mutex;
    #endif
#else
    #include <mutex>

    namespace shared_state_chrono = std::chrono;

    #if defined SHARED_STATE_NO_TIMEOUT
        #if defined SHARED_STATE_NO_SHARED_MUTEX
            class shared_state_mutex: public std::mutex {
            public:
                void lock_shared() { lock(); }
                bool try_lock_shared() { return try_lock(); }
                void unlock_shared() { unlock(); }

                bool try_lock_for(...) { return try_lock(); }
                bool try_lock_shared_for(...) { return try_lock(); }
            };
        #else
            class shared_state_mutex: public std::shared_mutex {
            public:
                bool try_lock_for(...) { return try_lock(); }
                bool try_lock_shared_for(...) { return try_lock_shared(); }
            };
        #endif
    #elif defined SHARED_STATE_NO_SHARED_MUTEX
        class shared_state_mutex: public std::timed_mutex {
        public:
            void lock_shared() { lock(); }
            bool try_lock_shared() { return try_lock(); }
            void unlock_shared() { unlock(); }

            template <class Rep, class Period>
            bool try_lock_shared_for(const shared_state_chrono::duration<Rep, Period>& rel_time) { return try_lock_for(rel_time); }
        };
    #else
        // typedef std::shared_timed_mutex shared_state_mutex; // Requires C++14
        #include "shared_timed_mutex_polyfill.h"
        typedef std_polyfill::shared_timed_mutex shared_state_mutex; // Requires C++11
    #endif
#endif


class no_lock_failed {};
class lock_failed: public virtual boost::exception, public virtual std::exception {
public:
    typedef boost::error_info<struct timeout, int> timeout_value;

    /**
     When a timeout occurs on a lock, shared_state makes an attempt to detect
     deadlocks. The thread with the timeout is blocked with another lock
     attempt long enough (same timeout as in the first attempt) for any other
     thread that is deadlocking with this thread to also fail its lock attempt.

     The try_again value describes whether that second lock attempt succeeded,
     but even if it succeeds lock_failed is still thrown.
     */
    typedef boost::error_info<struct try_again, bool> try_again_value;
};

struct shared_state_traits_default {
#ifdef _DEBUG
    // Disable timeouts during debug sessions by returning -1
    int timeout_ms() { return 100; }
    int verify_execution_time_ms() { return 50; }
#else
    int timeout_ms() { return 100; }
    // Disable verify_execution in "release" as it incurs an extra overhead
    int verify_execution_time_ms() { return -1; }
#endif
    VerifyExecutionTime::report report_func() { return 0; }
};

template<class C>
struct shared_state_traits: public shared_state_traits_default {};

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

    mutable shared_state_mutex lock;
    const int timeout_ms;
    const int verify_execution_time_ms;
    const VerifyExecutionTime::report report_func;
};


// compare with std::enable_if
template <bool, class Tp = void> struct disable_if {};
template <class T> struct disable_if<false, T> {typedef T type;};


/**
 * The shared_state class is a smart pointer that should guarantee thread-safe
 * access to objects, with
 *  - compile-time errors on missing locks,
 *  - run-time exceptions with backtraces on deadlocks and failed (timeout) locks,
 *  - run-time warnings on locks that are kept long enough to make it likely
 *    that other simultaneous lock attempts will fail.
 *
 *
 * In a nutshell
 * -------------
 *    shared_state<A> a{new A};
 * ...
 *    a.write()->foo();         // Mutually exclusive write access
 *
 *
 * Cheat sheet
 * -----------
 * The examples below uses this class declaration:
 *    class A {
 *    public:
 *       void foo();
 *       void bar() const;
 *       void baz() volatile;
 *    };
 *
 * Instanciated like this to make its data safe to use in concurrent threads:
 *    shared_state<A> a{new A};
 *
 * Shared read-only access:
 *    a.read()->bar();
 *
 * Enter critical section:
 *    {
 *        auto w = a.write();
 *        w->foo();
 *        w->bar();
 *    }
 *
 * Conditional critical section, don't wait for lock
 *    if (auto w = a.try_write())
 *    {
 *        w->foo();
 *    }
 *
 * Catch if a lock couldn't be obtained within a given timeout. The timeout is
 * set by shared_state_traits<A>:
 *    try {
 *       a.write()->foo();
 *    } catch(lock_failed) {
 *       ...
 *    }
 *
 * Use the volailte qualifier to denote thread-safe method that doesn't
 * require a lock:
 *    a->baz();
 *
 * For more complete examples (that actually compile) see
 * shared_state_test::test () in shared_state.cpp
 *
 *
 * Using shared_state
 * ------------------
 * Use shared_state to ensure thread-safe access to otherwise unprotected data
 * of class MyType by wrapping all 'MyType* p = new MyType' calls as:
 *
 *     shared_state<MyType> p{new MyType};
 *
 *
 * There are four ways to access the data in 'p'.
 *
 * 1. Thread-safe and exception-safe shared read-only access (lock and RAII)
 *
 *     p.read()->...
 *
 * or
 *     auto r = p.read();
 *     r->...
 *     r->...
 *
 *
 * 2. Thread-safe and exception-safe mutually exclusive read and write access
 *
 *     p.write()->...
 *
 * or
 *     auto w = p.write();
 *     w->...
 *     w->...
 *
 *
 * 3. Non thread-safe mutable state
 *
 *     shared_state<MyType>::shared_mutable_state m{p};
 *     m->...
 *     m->...
 *
 *
 * 4. Thread-safe access to methods that are declared as 'thread-safe' by using
 * the volatile qualifier on the method
 *
 *     p->...
 *
 * From a volatile object you can only access methods that are volatile (just
 * like only const methods are accessible from a const object). Using the
 * volatile classifier prevents access to use any "regular" (non-volatile)
 * methods.
 *
 * "The volatile keyword in C++11 ISO Standard code is to be used only for
 * hardware access; do not use it for inter-thread communication." [msdn]
 * shared_state doesn't use the volatile qualifier for inter-thread
 * communication but to ensure un-safe methods aren't called without adequate
 * locks. volailte is merely some sort of syntactic sugar in this context.
 *
 * The idea of letting volatile on methods denote 'thread-safe method' is based
 * on this article:
 * http://www.drdobbs.com/cpp/volatile-the-multithreaded-programmers-b/184403766
 *
 *
 * Performance and overhead
 * ------------------------
 * shared_state should cause an overhead of less than 0.1 microseconds in a
 * 'release' build when using 'no_lock_failed'.
 *
 * shared_state should fail fast when using 'no_lock_failed', within 0.1
 * microseconds in a 'release' build.
 *
 * shared_state should cause an overhead of less than 0.3 microseconds in a
 * 'release' build when 'verify_execution_time_ms <= 0'.
 *
 * shared_state should cause an overhead of less than 1.5 microseconds in a
 * 'release' build when 'verify_execution_time_ms > 0'.
 *
 *
 * Configuring timeouts
 * --------------------
 * It is possible to use different timeouts, different expected execution times
 * and to disable the timeout and expected execution time. Create a template
 * specialization of shared_state_traits to override the defaults. You can also
 * create an internal class called shared_state_traits within your type.
 *
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

    class lock_failed: public ::lock_failed {};

    shared_state () {}

    /**
     * If 'timeout_ms >= 0' read_ptr/write_ptr will try to lock until the timeout
     * has passed and then throw a lock_failed exception. If 'timeout_ms < 0'
     * they will block indefinitely until the lock becomes available.
     *
     * If 'verify_execution_time_ms >= 0' read_ptr/WritePTr will call
     * report_func if the lock is kept longer than this value. With the default
     * behaviour of VerifyExecutionTime if report_func = 0.
     */
    template<class Y,
             class = typename std::enable_if <std::is_convertible<Y*, element_type*>::value>::type>
    explicit shared_state ( Y* p )
    {
        reset(p);
    }

    template<class Y,
             class = typename std::enable_if <std::is_convertible<Y*, element_type*>::value>::type>
    shared_state(const shared_state<Y>& a)
        :
            p( a.p ),
            d( a.d )
    {
    }

    template<class Y,
             class = typename std::enable_if <std::is_convertible<Y*, element_type*>::value>::type>
    void reset( Y* yp=0 ) {
        if (yp)
        {
            p.reset (yp);
            d.reset (new details{typename shared_state_traits_helper< Y >::type()});
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

            return shared_state{std::shared_ptr<T>(), std::shared_ptr<details>()};
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
              px(const_cast<T*> (p.get ()))
        {}

        T* operator-> () const { return px; }
        T& operator* () const { return *px; }
        T* get () const { return px; }
        explicit operator bool() const { return px; }

    private:
        std::shared_ptr<volatile T> p;
        T* px;
    };

    /**
     * For examples of usage see void shared_state_test::test ().
     *
     * The purpose of read_ptr is to provide thread-safe access to an a const
     * object for a thread during the lifetime of the read_ptr. This access
     * may be shared by multiple threads that simultaneously use their own
     * read_ptr to access the same object.
     *
     * The accessors without no_lock_failed always returns an accessible
     * instance, never null. If a lock fails a lock_failed exception is thrown.
     *
     * @see void shared_state_test::test ()
     * @see class shared_state
     * @see class write_ptr
     */
    class read_ptr {
    public:
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
        explicit operator bool() const { return px; }

        void unlock() {
            if (px)
            {
                l.unlock_shared ();
                pc.reset ();
            }

            px = 0;
        }

    private:
        friend class shared_state;

        explicit read_ptr (const shared_state& vp)
            :   l (vp.readWriteLock()),
                p (vp.p),
                d (vp.d),
                px (const_cast<const T*> (vp.get ()))
        {
            lock ();
        }

        read_ptr (const shared_state& vp, no_lock_failed)
            :   l (vp.readWriteLock()),
                p (vp.p),
                d (vp.d),
                px (const_cast<const T*> (p.get ()))
        {
            if (!l.try_lock_shared ())
                px = 0;
            else
            {
                if (0<d->verify_execution_time_ms)
                    pc = VerifyExecutionTime::start (d->verify_execution_time_ms * 1e-3f, d->report_func);
            }
        }

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
            else if (l.try_lock_shared_for (shared_state_chrono::milliseconds{timeout_ms}))
            {
                // Got lock
            }
            else
            {
                // If this is a deadlock, make both threads throw by keeping this thread blocked.
                // See lock_failed::try_again
                bool try_again = l.try_lock_shared_for (shared_state_chrono::milliseconds{timeout_ms});
                if (try_again)
                    l.unlock_shared ();

                SHARED_STATE_THROW_EXCEPTION(lock_failed{}
                                      << typename lock_failed::timeout_value{timeout_ms}
                                      << typename lock_failed::try_again_value{try_again});
            }

            if (0<d->verify_execution_time_ms)
                pc = VerifyExecutionTime::start (d->verify_execution_time_ms * 1e-3f, d->report_func);
        }

        shared_state_mutex& l;
        // p is 'const volatile T', compared to shared_state::p which is just 'volatile T'.
        std::shared_ptr<const volatile T> p;
        std::shared_ptr<details> d;
        const T* px;
        VerifyExecutionTime::ptr pc;
    };


    /**
     * For examples of usage see void shared_state_test::test ().
     *
     * The purpose of write_ptr is to provide exclusive access to an object for
     * a single thread during the lifetime of the write_ptr.
     *
     * @see class read_ptr
     */
    class write_ptr {
    public:
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
        explicit operator bool() const { return px; }

        void unlock() {
            if (px)
            {
                l.unlock ();
                pc.reset ();
            }

            px = 0;
        }

    private:
        friend class shared_state;

        template<class = typename disable_if <std::is_convertible<const element_type*, element_type*>::value>::type>
        explicit write_ptr (const shared_state& vp)
            :   l (vp.readWriteLock()),
                p (vp.p),
                d (vp.d),
                px (const_cast<T*> (p.get ()))
        {
            lock ();
        }

        template<class = typename disable_if <std::is_convertible<const element_type*, element_type*>::value>::type>
        write_ptr (const shared_state& vp, no_lock_failed)
            :   l (vp.readWriteLock()),
                p (vp.p),
                d (vp.d),
                px (const_cast<T*> (p.get ()))
        {
            if (!l.try_lock ())
                px = 0;
            else
            {
                if (0<d->verify_execution_time_ms)
                    pc = VerifyExecutionTime::start (d->verify_execution_time_ms * 1e-3f, d->report_func);
            }
        }

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
            else if (l.try_lock_for (shared_state_chrono::milliseconds{timeout_ms}))
            {
            }
            else
            {
                bool try_again = l.try_lock_for (shared_state_chrono::milliseconds{timeout_ms});
                if (try_again)
                    l.unlock ();

                SHARED_STATE_THROW_EXCEPTION(lock_failed{}
                                      << typename lock_failed::timeout_value{timeout_ms}
                                      << typename lock_failed::try_again_value{try_again});
            }

            if (0<d->verify_execution_time_ms)
                pc = VerifyExecutionTime::start (d->verify_execution_time_ms * 1e-3f, d->report_func);
        }

        shared_state_mutex& l;
        std::shared_ptr<volatile T> p;
        std::shared_ptr<details> d;
        T* px;
        VerifyExecutionTime::ptr pc;
    };


    /**
     * @brief read provides thread safe read-only access.
     */
    read_ptr read() const volatile { return read_ptr(*const_cast<const shared_state*>(this)); }

    /**
     * @brief write provides thread safe read and write access. Not accessible
     * if T is const.
     */
    write_ptr write() volatile { return write_ptr(*const_cast<shared_state*>(this)); }

    /**
     * @brief try_read obtains the lock only if it is readily available.
     *
     * If the lock was not obtained it doesn't throw any exception, but the
     * accessors return null pointers. This function fails much faster (about
     * 30x faster) than setting timeout_ms=0 and discarding any lock_failed.
     */
    read_ptr try_read() const volatile { return read_ptr(*const_cast<const shared_state*>(this), no_lock_failed()); }

    /**
     * @brief try_write. See try_read.
     */
    write_ptr try_write() volatile { return write_ptr(*const_cast<shared_state*>(this), no_lock_failed()); }

    /**
     * @brief readWriteLock returns the mutex object for this instance.
     */
    shared_state_mutex& readWriteLock() const volatile { return (*const_cast<std::shared_ptr<details>*>(&d))->lock; }

private:
    template<typename Y>
    friend class shared_state;

    shared_state ( std::shared_ptr<volatile T> p, std::shared_ptr<details> d ) : p(p), d(d) {}

    std::shared_ptr<volatile T> p;
    std::shared_ptr<details> d;
};

class shared_state_test {
public:
    static void test ();
};

#endif // SHARED_STATE_H
