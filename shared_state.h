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


#if defined(SHARED_STATE_NO_TIMEOUT) || defined(SHARED_STATE_NO_SHARED_MUTEX) || defined(SHARED_STATE_BOOST_MUTEX)
    #include "shared_state_mutex.h"
#else
    namespace shared_state_chrono = std::chrono;
    #include "shared_timed_mutex_polyfill.h" // Only requires C++11, until std::shared_timed_mutex is available in C++14.
    typedef std_polyfill::shared_timed_mutex shared_state_mutex;
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

template<typename T>
struct shared_state_details: public shared_state_traits_helper<T>::type {
    shared_state_details() {}
    shared_state_details(shared_state_details const&) = delete;
    shared_state_details& operator=(shared_state_details const&) = delete;

    mutable shared_state_mutex lock;
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
 * Using shared_state
 * ------------------
 * Use shared_state to ensure thread-safe access to otherwise unprotected data
 * of class MyType by wrapping all 'MyType* p = new MyType' calls as:
 *
 *     shared_state<MyType> p{new MyType};
 *
 *
 * There are a couple of ways to access the data in 'p'. Call "p.write()" to
 * enter a critical section for read and write access. The critical section is
 * thread-safe and exception-safe through a mutex lock and RAII. p.write() can
 * be used either in a single function call:
 *
 *        p.write()->...
 *
 * Or to enter a critical section to perform a transaction over multiple method
 * calls:
 *
 *        {
 *          auto w = p.write();
 *          w->...
 *          w->...
 *        }
 *
 * Enter a critical section only if the lock is readily available:
 *
 *        if (auto w = p.try_write())
 *        {
 *          w->...
 *          w->...
 *        }
 *
 * Like-wise 'p.read()' or 'p.try_read()' creates a critical section with
 * shared read-only access. While these critical sections are sufficient for
 * the most part you might want to use some methods that take are responsible
 * for their own thread-safety. If you denoted such a method as thread-safe by
 * declaring the method with the volatile qualifier you can call it from
 * shared_state without a critical section. For instance, if the class
 * declaration looks like this:
 *
 *        class MyType {
 *        public:
 *          void foo() volatile;
 *          ...
 *        };
 *
 * You can then call foo() directly like this:
 *
 *        p->foo();
 *
 * An excepion is thrown if a lock couldn't be obtained within a given timeout.
 * The timeout is set, or disabled, by shared_state_traits<MyType>:
 *
 *        try {
 *          p.write()->...
 *        } catch(lock_failed) {
 *          ...
 *        }
 *
 * You can also discard the thread safety and get unprotected access to a
 * mutable state:
 *
 *        {
 *          shared_state<MyType>::shared_mutable_state m{p};
 *          m->...
 *          m->...
 *        }
 *
 * For more complete examples (that actually compile) see
 * shared_state_test::test () in shared_state.cpp.
 *
 *
 * Notes about using volatile
 * --------------------------
 * From a volatile instance you can only access methods that are volatile (just
 * like only const methods are accessible from a const instance). Using the
 * volatile classifier prevents access to use any "regular" (non-volatile)
 * methods. This applies to members as well but
 *
 * "The volatile keyword in C++11 ISO Standard code is to be used only for
 * hardware access; do not use it for inter-thread communication." [msdn]
 *
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
    typedef shared_state_details<typename std::remove_const<T>::type> details;

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
            d.reset (new details);
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
                if (0<d->verify_execution_time_ms())
                    pc = VerifyExecutionTime::start (d->verify_execution_time_ms() * 1e-3f, d->report_func());
            }
        }

        void lock() {
            int timeout_ms = d->timeout_ms(); // l is not locked, but timeout_ms is const

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

            if (0<d->verify_execution_time_ms())
                pc = VerifyExecutionTime::start (d->verify_execution_time_ms() * 1e-3f, d->report_func());
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
                if (0<d->verify_execution_time_ms())
                    pc = VerifyExecutionTime::start (d->verify_execution_time_ms() * 1e-3f, d->report_func());
            }
        }

        // See read_ptr::lock
        void lock() {
            int timeout_ms = d->timeout_ms();

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

            if (0<d->verify_execution_time_ms())
                pc = VerifyExecutionTime::start (d->verify_execution_time_ms() * 1e-3f, d->report_func());
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
