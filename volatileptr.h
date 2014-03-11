/**
 * The VolatilePtr class should guarantee thread-safe access to objects.
 * See details on the class VolatilePtr below.
 *
 * Author: johan.b.gustafsson@gmail.com
 */

#ifndef VOLATILELOCK_H
#define VOLATILELOCK_H

#include "verifyexecutiontime.h"

#include <boost/thread/shared_mutex.hpp>
#include <boost/exception/all.hpp>

#ifndef VOLATILEPTR_THROW_EXCEPTION
  #ifdef VOLATILEPTR_NO_BACKTRACE
    #define VOLATILEPTR_THROW_EXCEPTION(x) BOOST_THROW_EXCEPTION(x)
  #else
    #include "backtrace.h"
    #define VOLATILEPTR_THROW_EXCEPTION(x) BOOST_THROW_EXCEPTION(x << Backtrace::make (2))
  #endif
#endif

class NoLockFailed {};
class LockFailed: public virtual boost::exception, public virtual std::exception {
public:
    typedef boost::error_info<struct timeout, int> timeout_value;

    /**
     When a timeout occurs on a lock VolatilePtr makes an attempt to detect
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
//#define VolatilePtr_lock_timeout_ms -1
#define VolatilePtr_lock_timeout_ms 100
#else
#define VolatilePtr_lock_timeout_ms 100
#endif


template<class C>
struct VolatilePtrTypeTraits {
    int timeout_ms() { return VolatilePtr_lock_timeout_ms; }
    int verify_execution_time_ms() { return VolatilePtr_lock_timeout_ms/2; }
    VerifyExecutionTime::report report_func() { return 0; }
};

template<typename T>
struct has_volatile_ptr_traits
{
    // detect C::VolatilePtrTypeTraits::timeout_ms, assume
    // verify_execution_time_ms and report_func are also present
    template<typename C>
    static char test(decltype(&C::VolatilePtrTypeTraits::timeout_ms));

    template<typename C> // worst match
    static char (&test(...))[2];

    static const bool value = (sizeof( test<T>(0)  ) == 1);
};

template<class C, bool a = has_volatile_ptr_traits<C>::value>
struct VolatilePtrTraitsType;

template<class C>
struct VolatilePtrTraitsType<C,false> {
    typedef VolatilePtrTypeTraits<C> type;
};

template<class C>
struct VolatilePtrTraitsType<C,true> {
    typedef typename C::VolatilePtrTypeTraits type;
};

class VolatilePtrDetails {
    template<typename Y> friend class VolatilePtr;

    template<typename Traits>
    VolatilePtrDetails(Traits traits)
        : timeout_ms(traits.timeout_ms()),
          verify_execution_time_ms(traits.verify_execution_time_ms()),
          report_func(traits.report_func())
    {}

    VolatilePtrDetails(VolatilePtrDetails const&) = delete;
    VolatilePtrDetails& operator=(VolatilePtrDetails const&) = delete;

    mutable boost::shared_mutex lock;
    const int timeout_ms;
    const int verify_execution_time_ms;
    const VerifyExecutionTime::report report_func;
};


/**
 * The VolatilePtr class should guarantee thread-safe access to objects, with
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
 *    };
 *
 *    VolatilePtr<A> a(new A); // Smart pointer that makes its data safe to use
 *                             // in multiple threads.
 *    a->foo();  <-- error     // un-safe method call fails in compile time
 *    write1(a)->foo();        // Mutally exclusive write access
 *    read1(a)->bar();         // Simultaneous read-only access to const methods
 *
 *    try {
 *       WritePtr(a)->foo();
 *    } catch(LockFailed) {
 *       // Catch if a lock couldn't be obtained within a given timeout.
 *       // The timeout is set when instanciating VolatilePtr<A>.
 *    }
 *
 *    A::WritePtr w(a, NoLockFailed()); // Returns immediately without waiting.
 *    if (w) w->foo();  // Only lock for access if readily availaible (i.e
 *                      // currently not locked by any other thread)
 *
 * For more complete examples see:
 *    VolatilePtrTest::test ()
 *
 *
 * To use VolatilePtr to ensure thread-safe access to some previously
 * un-protected data of class MyType, wrap all 'new MyType' calls as arguments
 * to the constructor of VolatilePtr<MyType>. Like in the example above for
 * 'class A'.
 *
 * The idea is let the 'volatile' qualifier denote that an object can be
 * modified by any thread at any time and use a pointer to a volatile object
 * when juggling references to objects. When you need the data you access it
 * safely through VolatilePtr::ReadPtr and VolatilePtr::WritePtr.
 *
 * From a volatile object you can only access methods that are volatile (just
 * like only const methods are accessible from a const object). Using the
 * volatile classifier prevents access to use any "regular" (non-volatile)
 * methods.
 *
 * The helper classes ReadPtr and WritePtr uses RAII provides both thread-safe
 * and exception-safe access to a non-volatile reference to the object.
 *
 * Regarding "The volatile keyword in C++11 ISO Standard code is to be used
 * only for hardware access; do not use it for inter-thread communication."
 * VolatilePtr doesn't use the volatile qualifier for inter-thread
 * communication but to ensure data isn't accessed without adequate locks. The
 * volatile qualifier is normally casted away in WritePtr/ReadPtr by the time
 * any data is dereferenced.
 *
 *
 * VolatilePtr should cause an overhead of less than 0.1 microseconds in a
 * 'release' build when using 'NoLockFailed'.
 *
 * VolatilePtr should fail fast when using 'NoLockFailed', within 0.1
 * microseconds in a 'release' build.
 *
 * VolatilePtr should cause an overhead of less than 0.3 microseconds in a
 * 'release' build when 'verify_execution_time_ms < 0'.
 *
 * VolatilePtr should cause an overhead of less than 1.5 microseconds in a
 * 'release' build when 'verify_execution_time_ms >= 0'.
 *
 *
 * It is possible to use different timeouts, different expected execution times
 * and to disable the timeout and expected execution time. Create a template
 * specialization of VolatilePtrTypeTraits to override the defaults.
 *
 * TODO rename to shared_state or shared_mutable_state
 *
 * Author: johan.b.gustafsson@gmail.com
 */
template<typename T>
class VolatilePtr final
{
private:
    typedef const VolatilePtrDetails details;

public:
    typedef T element_type;
    typedef boost::shared_mutex shared_mutex;

    class LockFailed: public ::LockFailed {};

    VolatilePtr () {}

    /**
     * If 'timeout_ms >= 0' ReadPtr/WritePtr will try to look until the timeout
     * has passed and then throw a LockFailed exception. If 'timeout_ms < 0'
     * they will block indefinitely until the lock becomes available.
     *
     * If 'verify_execution_time_ms >= 0' ReadPtr/WritePTr will call
     * report_func if the lock is kept longer than this value. With the default
     * behaviour of VerifyExecutionTime if report_func = 0.
     */
    template<class Y,
             class = typename std::enable_if
                     <
                        std::is_convertible<Y*, element_type*>::value
                     >::type
            >
    explicit VolatilePtr ( Y* p )
    {
        reset(p);
    }

    template<class Y,
             class = typename std::enable_if
                     <
                        std::is_convertible<Y*, element_type*>::value
                     >::type
            >
    VolatilePtr(VolatilePtr<Y> a)
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
            d.reset (new details(typename VolatilePtrTraitsType< Y >::type()));
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
    bool operator== (const VolatilePtr& b) const { return p.get () == b.p.get (); }
    bool operator!= (const VolatilePtr& b) const { return !(*this == b); }
    bool operator < (const VolatilePtr& b) const { return this->p < b.p; }

    class WeakPtr {
    public:
        WeakPtr() {}
        WeakPtr(const VolatilePtr& t) : data_(t.d), p_(t.p) {}

        VolatilePtr lock() const {
            std::shared_ptr<details> datap = data_.lock ();
            std::shared_ptr<volatile T> pp = p_.lock ();

            if (pp && datap)
                return VolatilePtr(pp, datap);

            return VolatilePtr(std::shared_ptr<volatile T>(), std::shared_ptr<details>());
        }

    private:
        std::weak_ptr<details> data_;
        std::weak_ptr<volatile T> p_;
    };

    /**
     * For examples of usage see void VolatilePtrTest::test ().
     *
     * The purpose of ReadPtr is to provide thread-safe access to an a const
     * object for a thread during the lifetime of the ReadPtr. This access
     * may be shared by multiple threads that simultaneously use their own
     * ReadPtr to access the same object.
     *
     * The accessors without NoLockFailed always returns an accessible
     * instance, never null. If a lock fails a LockFailed exception is thrown.
     *
     * @see void VolatilePtrTest::test ()
     * @see class VolatilePtr
     * @see class WritePtr
     */
    class ReadPtr {
    public:
        template<class Y,
                 class = typename std::enable_if
                         <
                            std::is_convertible<Y*, const element_type*>::value
                         >::type
                >
        explicit ReadPtr (const VolatilePtr<Y>& vp)
            :   l (vp.readWriteLock()),
                p (vp.p),
                d (vp.d),
                px (const_cast<const T*> (vp.get ()))
        {
            lock ();
        }

        /**
         * @brief ReadPtr with NoLockFailed obtains the lock if it is readily
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
        explicit ReadPtr (const VolatilePtr<Y>& vp, NoLockFailed)
            :   l (vp.readWriteLock()),
                p (vp.p),
                d (vp.d),
                px (const_cast<const T*> (p.get ()))
        {
            if (!l->try_lock_shared ())
                px = 0;
        }

        // The copy constructor is not implemented anywhere and ReadPtr is not
        // copyable. But if a there is a public copy constructor the compiler
        // can perform return value optimization in read1 and write1.
        ReadPtr(const ReadPtr&);
        ReadPtr& operator=(ReadPtr const&) = delete;

        ~ReadPtr() {
            // The destructor isn't called if the constructor throws.
            unlock ();
        }

        const T* operator-> () const { return px; }
        const T& operator* () const { return *px; }
        const T* get () const { return px; }

        void unlock() {
            if (px)
            {
                l->unlock_shared ();
                pc.reset ();
            }

            px = 0;
        }

    private:
        void lock() {
            int timeout_ms = d->timeout_ms; // l is not locked, but timeout_ms is const

            // try_lock_shared_for and lock_shared are unnecessarily complex if
            // the lock is available right away
            if (l->try_lock_shared ())
            {
                // Got lock
            }
            else if (timeout_ms < 0)
            {
                l->lock_shared ();
                // Got lock
            }
            else if (l->try_lock_shared_for (boost::chrono::milliseconds(timeout_ms)))
            {
                // Got lock
            }
            else
            {
                // If this is a deadlock, make both threads throw by keeping this thread blocked.
                // See LockFailed::try_again
                bool try_again = l->try_lock_shared_for (boost::chrono::milliseconds(timeout_ms));
                if (try_again)
                    l->unlock_shared ();

                VOLATILEPTR_THROW_EXCEPTION(LockFailed()
                                      << typename LockFailed::timeout_value(timeout_ms)
                                      << typename LockFailed::try_again_value(try_again));
            }

            if (0<d->verify_execution_time_ms)
                pc = VerifyExecutionTime::start (d->verify_execution_time_ms * 1e-3f, d->report_func);
        }

        shared_mutex* l;
        // p is 'const volatile T', compared to VolatilePtr::p which is just 'volatile T'.
        std::shared_ptr<const volatile T> p;
        std::shared_ptr<details> d;
        const T* px;
        VerifyExecutionTime::Ptr pc;
    };


    /**
     * For examples of usage see void VolatilePtrTest::test ().
     *
     * The purpose of WritePtr is to provide exclusive access to an object for
     * a single thread during the lifetime of the WritePtr.
     *
     * @see class ReadPtr
     */
    class WritePtr {
    public:
        template<class Y,
                 class = typename std::enable_if
                         <
                            std::is_convertible<Y*, element_type*>::value
                         >::type
                >
        explicit WritePtr (const VolatilePtr<Y>& vp)
            :   l (vp.readWriteLock()),
                p (vp.p),
                d (vp.d),
                px (const_cast<T*> (p.get ()))
        {
            lock ();
        }

        // See ReadPtr(const volatile ReadPtr&, NoLockFailed)
        template<class Y,
                 class = typename std::enable_if
                         <
                            std::is_convertible<Y*, element_type*>::value
                         >::type
                >
        explicit WritePtr (const VolatilePtr<Y>& vp, NoLockFailed)
            :   l (vp.readWriteLock()),
                p (vp.p),
                d (vp.d),
                px (const_cast<T*> (p.get ()))
        {
            if (!l->try_lock ())
                px = 0;
        }

        // See ReadPtr(const ReadPtr&)
        WritePtr(const WritePtr&);
        WritePtr& operator=(WritePtr const&) = delete;

        ~WritePtr() {
            unlock ();
        }

        T* operator-> () const { return px; }
        T& operator* () const { return *px; }
        T* get () const { return px; }
        VolatilePtr getPtr () const { return p; }

        void unlock() {
            if (px)
            {
                l->unlock ();
                pc.reset ();
            }

            px = 0;
        }

    private:
        // See ReadPtr::lock
        void lock() {
            int timeout_ms = d->timeout_ms;

            if (l->try_lock())
            {
            }
            else if (timeout_ms < 0)
            {
                l->lock ();
            }
            else if (l->try_lock_for (boost::chrono::milliseconds(timeout_ms)))
            {
            }
            else
            {
                bool try_again = l->try_lock_for (boost::chrono::milliseconds(timeout_ms));
                if (try_again)
                    l->unlock ();

                VOLATILEPTR_THROW_EXCEPTION(LockFailed()
                                      << typename LockFailed::timeout_value(timeout_ms)
                                      << typename LockFailed::try_again_value(try_again));
            }

            if (0<d->verify_execution_time_ms)
                pc = VerifyExecutionTime::start (d->verify_execution_time_ms * 1e-3f, d->report_func);
        }

        shared_mutex* l;
        std::shared_ptr<volatile T> p;
        std::shared_ptr<details> d;
        T* px;
        VerifyExecutionTime::Ptr pc;
    };


//    ReadPtr read() volatile const { return ReadPtr(*this); }
//    WritePtr write() volatile { return WritePtr(*this); }

    /**
     * @brief readWriteLock returns the shared_mutex* object for this instance.
     */
    // TODO change to shared_mutex&
    shared_mutex* readWriteLock() const volatile { return &(*const_cast<std::shared_ptr<details>*>(&d))->lock; }

private:
    template<typename Y>
    friend class VolatilePtr;

    explicit VolatilePtr ( std::shared_ptr<volatile T> p, std::shared_ptr<details> d ) : p(p), d(d) {}

    std::shared_ptr<volatile T> p;
    std::shared_ptr<details> d;
};


template<typename T>
typename VolatilePtr<T>::WritePtr write1( const VolatilePtr<T>& t) {
    return typename VolatilePtr<T>::WritePtr(t);
}

template<typename T>
typename VolatilePtr<T>::WritePtr write1( volatile const VolatilePtr<T>& t) {
    return typename VolatilePtr<T>::WritePtr(t);
}

template<typename T>
typename VolatilePtr<T>::ReadPtr read1( const VolatilePtr<T>& t) {
    return typename VolatilePtr<T>::ReadPtr(t);
}

template<typename T>
typename VolatilePtr<T>::ReadPtr read1( volatile const VolatilePtr<T>& t) {
    return typename VolatilePtr<T>::ReadPtr(t);
}

class VolatilePtrTest {
public:
    static void test ();
};

#endif // VOLATILELOCK_H
