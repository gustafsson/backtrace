#ifndef VOLATILELOCK_H
#define VOLATILELOCK_H

#include "unused.h"
#include "verifyexecutiontime.h"

#include <boost/thread/shared_mutex.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
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

/**
 * The VolatilePtr class should guarantee thread-safe access to objects, with
 *  - compile-time errors on missing locks
 *  - run-time exceptions with backtraces on deadlocks and failed locks
 *  - run-time warnings on locks that are likely to cause other lock attempts to fail.
 *
 * Quick cheat sheet:
 *    class A: public VolatilePtr<A> {
 *    public:
 *       void foo();
 *       void bar() const;
 *    };
 *
 *    A::Ptr a( new A); // Smart pointer safe to use in multiple threads
 *    a->foo();         // un-safe method call fails in compile time
 *    write1(a)->foo(); // Mutally exclusive write access
 *    read1(a)->bar();  // Simultaneous read-only access to const methods
 *
 *    try {
 *       WritePtr(a, T)->foo();
 *    } catch(LockFailed) {
 *       // Catch if a lock couldn't be obtained within T milliseconds.
 *       // Default timeout is VolatilePtr_lock_timeout_ms.
 *       // A negative timeout means wait indefinitely.
 *    }
 *
 *    A::WritePtr w(a, NoLockFailed());
 *    if (w) w->foo();  // Only lock for access if readily availaible (i.e
 *                      // currently not locked by any other thread)
 *
 * For more complete examples see:
 *    VolatilePtrTest::test ()
 *
 * To use VolatilePtr to manage thread-safe access to some previously
 * un-protected data you first need to create a new class representing the
 * state which needs to be managed. If this is just a single existing class
 * you can either
 *  1) change the existing class to make it inherit VolatilePtr<ClassType>,
 *  2) create a wrapper class.
 *
 * The idea is to use a pointer to a volatile object when juggling references to
 * objects. From a volatile object you can only access methods that are volatile
 * (just like only const methods are accessible from a const object). Using the
 * volatile classifier prevents access to use any "regular" (non-volatile) methods.
 *
 * The helper classes ReadPtr and WritePtr uses RAII for thread-safe and
 * exception-safe access to a non-volatile reference to the object.
 *
 *
 * VolatilePtr should cause an overhead of less than 0.1 microseconds in a
 * 'release' build when using 'NoLockFailed'.
 *
 * VolatilePtr should cause an overhead of less than 0.3 microseconds in a
 * 'release' build when 'verify_execution_time_ms < 0'.
 *
 * VolatilePtr should cause an overhead of less than 1.5 microseconds in a
 * 'release' build when 'verify_execution_time_ms >= 0'.
 *
 * See VolatilePtr::ReadPtr for details on the arguments to the accessors.
 *
 * Author: johan.b.gustafsson@gmail.com
 *
 * TODO VolatilePtr shouldn't require modification of the class T.
 */
template<typename T>
class VolatilePtr
{
protected:

    /**
     * VolatilePtr needs to be instantiated as a subclass.
     *
     * If 'timeout_ms >= 0' ReadPtr/WritePtr will try to look until the timeout
     * has passed and then throw a LockFailed exception. If 'timeout_ms < 0'
     * they will block indefinitely until the lock becomes available.
     *
     * If 'verify_execution_time_ms >= 0' ReadPtr/WritePTr will call
     * report_func if the lock is kept longer than this value. With the default
     * behaviour of VerifyExecutionTime if report_func = 0.
     */
    VolatilePtr (int timeout_ms = VolatilePtr_lock_timeout_ms,
                 int verify_execution_time_ms = VolatilePtr_lock_timeout_ms/2,
                 VerifyExecutionTime::report report_func = 0)
        :
          timeout_ms_(timeout_ms),
          verify_execution_time_ms_(verify_execution_time_ms),
          report_func_(report_func)
    {}


    /**
     * Call setTimeOuts during construction only, it is undefined behaviour to
     * change timeout_ms_ later as it needs to be accessed before locking the
     * mutex.
     *
     * Ideally the constructor arguments would be enough, but that would be too
     * impractical.
     */
    void setTimeOuts (int timeout_ms, int verify_execution_time_ms, VerifyExecutionTime::report report_func=0) {
        *const_cast<int*>(&timeout_ms_) = timeout_ms;
        verify_execution_time_ms_ = verify_execution_time_ms;
        report_func_ = report_func;
    }

public:

    typedef T element_type;
    typedef boost::shared_ptr<volatile T> Ptr;
    typedef boost::shared_ptr<const volatile T> ConstPtr;
    typedef boost::weak_ptr<volatile T> WeakPtr;
    typedef boost::weak_ptr<const volatile T> WeakConstPtr;
    typedef boost::shared_mutex shared_mutex;

    class LockFailed: public ::LockFailed {};

    VolatilePtr(VolatilePtr const&) = delete;
    VolatilePtr& operator=(VolatilePtr const&) = delete;

    ~VolatilePtr () {
        UNUSED(VolatilePtr* p) = (T*)0; // T is required to be a subtype of VolatilePtr<T>
    }

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
        explicit ReadPtr (const Ptr& p)
            :   l_ (p->readWriteLock()),
                p_ (p),
                t_ (const_cast<const T*> (p.get ()))
        {
            lock ();
        }

        explicit ReadPtr (const volatile Ptr& p)
            :   l_ ((*const_cast<const Ptr*> (&p))->readWriteLock()),
                p_ (*const_cast<const Ptr*> (&p)),
                t_ (const_cast<const T*> (const_cast<const Ptr*> (&p)->get ()))
        {
            lock ();
        }

        explicit ReadPtr (const volatile ConstPtr& p)
            :   l_ ((*const_cast<const ConstPtr*> (&p))->readWriteLock()),
                p_ (*const_cast<const ConstPtr*> (&p)),
                t_ (const_cast<const T*> (const_cast<const ConstPtr*> (&p)->get ()))
        {
            lock ();
        }

        explicit ReadPtr (const volatile T* p)
            :   l_ (p->readWriteLock()),
                t_ (const_cast<const T*> (p))
        {
            lock ();
        }

        /**
         * @brief ReadPtr with NoLockFailed obtains the lock if it is readily available.
         * If the lock was not obtained it doesn't throw any exception, but the accessors
         * returns a null pointer. This function is 1000 times faster than setting
         * timeout_ms=0 and discarding any LockFailed; i.e.
         *
         * ReadPtr w(p,NoLockFailed()); if(w) { ... }
         * // Takes about 20 ns.
         *
         * try {ReadPtr w(p,0); ... } catch(const LockFailed&) {}
         * // Takes about 1 us.
         *
         * @param p
         */
        explicit ReadPtr (const volatile Ptr& p, NoLockFailed)
            :   l_ ((*const_cast<const Ptr*> (&p))->readWriteLock()),
                p_ (*const_cast<const Ptr*> (&p)),
                t_ (const_cast<const T*> (const_cast<const Ptr*> (&p)->get ()))
        {
            if (!l_->try_lock_shared ())
                t_ = 0;
        }

        explicit ReadPtr (const volatile ConstPtr& p, NoLockFailed)
            :   l_ ((*const_cast<const ConstPtr*> (&p))->readWriteLock()),
                p_ (*const_cast<const ConstPtr*> (&p)),
                t_ (const_cast<const T*> (const_cast<const ConstPtr*> (&p)->get ()))
        {
            if (!l_->try_lock_shared ())
                t_ = 0;
        }

        // Can't implement a copy constructor as ReadPtr wouldn't be able to
        // maintain the lock through a copy.
        //
        // The copy constructor is not implemented anywhere and ReadPtr is not
        // copyable. But if a there is a public copy constructor the compiler
        // can perform return value optimization in read1 and write1.
        ReadPtr(const ReadPtr&);
        ReadPtr& operator=(ReadPtr const&) = delete;

        ~ReadPtr() {
            // The destructor isn't called if the constructor throws.
            unlock ();
        }

        const T* operator-> () const { return t_; }
        const T& operator* () const { return *t_; }
        const T* get () const { return t_; }
        ConstPtr getPtr () const { return p_; }

        void unlock() {
            if (t_)
            {
                l_->unlock_shared ();
                pc_.reset ();
            }

            t_ = 0;
        }

    private:
        // This constructor is not implemented as it's an error to pass a
        // 'const T*' parameter to ReadPTr. If the caller has such a pointer
        // it should use it directly rather than trying to lock it again.
        ReadPtr(T*);
        ReadPtr(const T*);

        void lock() {
            int timeout_ms = t_->timeout_ms_; // t_ is not locked, but timeout_ms_ is const

            // try_lock_shared_for and lock_shared are unnecessarily complex if
            // the lock is available right away
            if (l_->try_lock_shared ())
            {
                // Got lock
            }
            else if (timeout_ms < 0)
            {
                l_->lock_shared ();
                // Got lock
            }
            else if (l_->try_lock_shared_for (boost::chrono::milliseconds(timeout_ms)))
            {
                // Got lock
            }
            else
            {
                // If this is a deadlock, make both threads throw by keeping this thread blocked.
                // See LockFailed::try_again
                bool try_again = l_->try_lock_shared_for (boost::chrono::milliseconds(timeout_ms));
                if (try_again)
                    l_->unlock_shared ();

                VOLATILEPTR_THROW_EXCEPTION(LockFailed()
                                      << typename LockFailed::timeout_value(timeout_ms)
                                      << typename LockFailed::try_again_value(try_again));
            }

            if (0<t_->verify_execution_time_ms_)
                pc_ = VerifyExecutionTime::start (t_->verify_execution_time_ms_*1e-3f, t_->report_func_);
        }

        shared_mutex* l_;
        const ConstPtr p_;
        const T* t_;
        VerifyExecutionTime::Ptr pc_;
    };


    /**
     * For examples of usage see void VolatilePtrTest::test ().
     *
     * The purpose of WritePtr is to provide exclusive access to an object for
     * a single thread during the lifetime of the WritePtr.
     *
     * @see void VolatilePtrTest::test ()
     * @see class VolatilePtr
     * @see class ReadPtr
     */
    class WritePtr {
    public:
        explicit WritePtr (const Ptr& p)
            :   l_ (p->readWriteLock()),
                p_ (p),
                t_ (const_cast<T*> (p.get ()))
        {
            lock ();
        }

        explicit WritePtr (const volatile Ptr& p)
            :   l_ ((*const_cast<const Ptr*> (&p))->readWriteLock()),
                p_ (*const_cast<const Ptr*> (&p)),
                t_ (const_cast<T*> (const_cast<const Ptr*> (&p)->get ()))
        {
            lock ();
        }

        explicit WritePtr (volatile T* p)
            :   l_ (p->readWriteLock()),
                t_ (const_cast<T*> (p))
        {
            lock ();
        }

        // See ReadPtr(const volatile ReadPtr&, NoLockFailed)
        explicit WritePtr (const Ptr& p, NoLockFailed)
            :   l_ (p->readWriteLock()),
                p_ (p),
                t_ (const_cast<T*> (p.get ()))
        {
            if (!l_->try_lock ())
                t_ = 0;
        }

        explicit WritePtr (const volatile Ptr& p, NoLockFailed)
            :   l_ ((*const_cast<const Ptr*> (&p))->readWriteLock()),
                p_ (*const_cast<const Ptr*> (&p)),
                t_ (const_cast<T*> (const_cast<const Ptr*> (&p)->get ()))
        {
            if (!l_->try_lock ())
                t_ = 0;
        }

        // See ReadPtr(const ReadPtr&)
        WritePtr(const WritePtr&);
        WritePtr& operator=(WritePtr const&) = delete;

        ~WritePtr() {
            unlock ();
        }

        T* operator-> () const { return t_; }
        T& operator* () const { return *t_; }
        T* get () const { return t_; }
        Ptr getPtr () const { return p_; }

        void unlock() {
            if (t_)
            {
                l_->unlock ();
                pc_.reset ();
            }

            t_ = 0;
        }

    private:
        // See ReadPtr(const T*)
        WritePtr (T* p);

        // See ReadPtr::lock
        void lock() {
            int timeout_ms = t_->timeout_ms_; // t_ is not locked, but timeout_ms_ is const

            if (l_->try_lock())
            {
            }
            else if (timeout_ms < 0)
            {
                l_->lock ();
            }
            else if (l_->try_lock_for (boost::chrono::milliseconds(timeout_ms)))
            {
            }
            else
            {
                bool try_again = l_->try_lock_for (boost::chrono::milliseconds(timeout_ms));
                if (try_again)
                    l_->unlock ();

                VOLATILEPTR_THROW_EXCEPTION(LockFailed()
                                      << typename LockFailed::timeout_value(timeout_ms)
                                      << typename LockFailed::try_again_value(try_again));
            }

            if (0<t_->verify_execution_time_ms_)
                pc_ = VerifyExecutionTime::start (t_->verify_execution_time_ms_*1e-3f, t_->report_func_);
        }

        shared_mutex* l_;
        const Ptr p_;
        T* t_;
        VerifyExecutionTime::Ptr pc_;
    };


    /**
      This would be handy.
      ReadPtr read() volatile const { return ReadPtr(this);}
      WritePtr write() volatile { return WritePtr(this);}
      */
protected:
    /**
     * @brief readWriteLock
     * ok to cast away volatile to access a shared_mutex (it's kind of the
     * point of shared_mutex that it can be accessed from multiple threads
     * simultaneously)
     * @return the shared_mutex* object for this instance.
     */
    shared_mutex* readWriteLock() const volatile { return const_cast<shared_mutex*>(&lock_); }


private:
    /**
     * @brief lock_
     *
     * For examples of usage see
     * void VolatilePtrTest::test ();
     */
    mutable shared_mutex lock_;
    const int timeout_ms_;
    int verify_execution_time_ms_;
    VerifyExecutionTime::report report_func_;
};


template<typename T>
typename T::WritePtr write1( const boost::shared_ptr<volatile T>& t) {
    return typename T::WritePtr(t);
}

template<typename T>
typename T::WritePtr write1( volatile const boost::shared_ptr<volatile T>& t) {
    return typename T::WritePtr(t);
}

template<typename T>
typename T::ReadPtr read1( const boost::shared_ptr<volatile T>& t) {
    return typename T::ReadPtr(t);
}

template<typename T>
typename T::ReadPtr read1( volatile const boost::shared_ptr<volatile T>& t) {
    return typename T::ReadPtr(t);
}

class VolatilePtrTest {
public:
    static void test ();
};

#endif // VOLATILELOCK_H
