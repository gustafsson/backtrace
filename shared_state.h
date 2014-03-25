/**
 * The shared_state class should guarantee thread-safe access to objects.
 * See details on the class shared_state below.
 *
 * Author: johan.b.gustafsson@gmail.com
 */

#ifndef SHARED_STATE_H
#define SHARED_STATE_H

template<class T>
class shared_state;


#if defined(SHARED_STATE_NO_TIMEOUT) || defined(SHARED_STATE_NO_SHARED_MUTEX) || defined(SHARED_STATE_BOOST_MUTEX)
    #include "shared_state_mutex.h"
#else
    #include "shared_timed_mutex_polyfill.h" // Only requires C++11, until std::shared_timed_mutex is available in C++14.
    namespace shared_state_chrono = std::chrono;
    typedef std_polyfill::shared_timed_mutex shared_state_mutex;
#endif


class lock_failed: public virtual std::exception {};


struct shared_state_traits_default {
    /**
     If 'timeout >= 0' read_ptr/write_ptr will try to lock until the timeout
     has passed and then throw a lock_failed exception. If 'timeout < 0'
     they will block indefinitely until the lock becomes available.

     Define SHARED_STATE_NO_TIMEOUT to disable timeouts altogether, all lock
     attempts will then either fail or succeed immediately.

     timeout() must be reentrant, i.e thread-safe without the support of
     shared_state.
     */
    double timeout () { return 0.100; }

    /**
     When a timeout occurs on a lock, shared_state makes an attempt to detect
     deadlocks. The thread with the timeout is blocked with another lock
     attempt long enough (same timeout as in the first attempt) for any other
     thread that is deadlocking with this thread to also fail its lock attempt.

     The try_again value describes whether that second lock attempt succeeded,
     but even if it succeeds lock_failed is still thrown.
     */
    template<class C>
    void timeout_failed (double timeout, int try_again) {
        (void)timeout;
        (void)try_again;
        throw typename shared_state<C>::lock_failed{};
    }

    /**
     was_locked and was_unlocked are called each time the mutex for this is
     instance is locked and unlocked, respectively. Regardless if the lock
     is a read-only lock or a read-and-write lock.
     */
    void was_locked () {}
    void was_unlocked () {}
};

template<class C>
struct shared_state_traits: public shared_state_traits_default {};

template<typename T>
struct has_shared_state_traits
{
    // detect C::shared_state_traits::timeout, assume
    // verify_execution_time and report_func are also present
    template<typename C>
    static char test(decltype(&C::shared_state_traits::timeout));

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
 *  - run-time exceptions on lock timeout, from all racing threads
 *    participating in a deadlock.
 *
 * It should be extensible enough to let clients efficiently add features like
 *  - backtraces on failed locks,
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
 * shared read-only access. An excepion is thrown if a lock couldn't be
 * obtained within a given timeout. The timeout is set, or disabled, by
 * shared_state_traits<MyType>:
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
 *          auto m = p.unprotected();
 *          m->...
 *          m->...
 *        }
 *
 * For more complete examples (that actually compile) see
 * shared_state_test::test () in shared_state.cpp.
 *
 *
 * Performance and overhead
 * ------------------------
 * shared_state should cause an overhead of less than 0.1 microseconds in a
 * 'release' build when using 'try_write' or 'try_read'.
 *
 * shared_state should fail within 0.1 microseconds in a 'release' build when
 * using 'try_write' or 'try_read' on a busy lock.
 *
 * shared_state should cause an overhead of less than 0.3 microseconds in a
 * 'release' build when using 'write' or 'read'.
 *
 *
 * Configuring timeouts and extending functionality
 * ------------------------------------------------
 * It is possible to use different timeouts, or disable timeouts, for different
 * types. Create a template specialization of shared_state_traits to override
 * the defaults. Alternatively you can also create an internal class called
 * shared_state_traits within your type. See 'shared_state_traits_default' for
 * more details.
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

    template<class Y,
             class = typename std::enable_if <std::is_convertible<Y*, element_type*>::value>::type>
    explicit shared_state ( Y* p )
    {
        reset(p);
    }

    template<class Y,
             class = typename std::enable_if <std::is_convertible<Y*, element_type*>::value>::type>
    explicit shared_state ( Y* p, std::shared_ptr<details> dp )
    {
        reset(p, dp);
    }

    template<class Y,
             class = typename std::enable_if <std::is_convertible<Y*, element_type*>::value>::type>
    shared_state(const shared_state<Y>& a)
        :
            p( a.p ),
            d( a.d )
    {
    }

    void reset() {
        p.reset ();
        d.reset ();
    }

    template<class Y,
             class = typename std::enable_if <std::is_convertible<Y*, element_type*>::value>::type>
    void reset( Y* yp ) {
        p.reset (yp);
        d.reset (new details);
    }

    template<class Y,
             class = typename std::enable_if <std::is_convertible<Y*, element_type*>::value>::type>
    void reset( Y* yp, std::shared_ptr<details> dp) {
        p.reset (yp);
        d.swap (dp);
    }

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
            std::shared_ptr<T> pp = p.lock ();

            // Either one could be kept separately from
            // shared_state::unprotected() or shared_state::traits().
            // Need both to reconstruct shared_state.
            if (pp && datap)
                return shared_state(pp, datap);

            return shared_state{std::shared_ptr<T>(), std::shared_ptr<details>()};
        }

    private:
        std::weak_ptr<details> data;
        std::weak_ptr<T> p;
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
            :   l (b.l)
        {
            p.swap (b.p);
            d.swap (b.d);
        }

        read_ptr(const read_ptr&) = delete;
        read_ptr& operator=(read_ptr const&) = delete;

        ~read_ptr() {
            // The destructor isn't called if the constructor throws.
            unlock ();
        }

        const T* operator-> () const { return p.get (); }
        const T& operator* () const { return *p; }
        const T* get () const { return p.get (); }
        explicit operator bool() const { return (bool)p; }

        void unlock() {
            if (p)
            {
                l.unlock_shared ();
                d->was_unlocked();
            }

            p.reset ();
        }

    private:
        friend class shared_state;

        explicit read_ptr (const shared_state& vp)
            :   l (vp.readWriteLock()),
                p (vp.p),
                d (vp.d)
        {
            lock ();
        }

        read_ptr (const shared_state& vp, bool)
            :   l (vp.readWriteLock()),
                p (vp.p),
                d (vp.d)
        {
            if (!l.try_lock_shared ())
                p.reset ();
            else
                d->was_locked();
        }

        void lock() {
            double timeout = d->timeout(); // l is not locked, but timeout is required to be reentrant

            // try_lock_shared_for and lock_shared are unnecessarily complex if
            // the lock is available right away
            if (l.try_lock_shared ())
            {
                // Got lock
            }
            else if (timeout < 0)
            {
                l.lock_shared ();
                // Got lock
            }
            else if (l.try_lock_shared_for (shared_state_chrono::duration<double>{timeout}))
            {
                // Got lock
            }
            else
            {
                // If this is a deadlock, make both threads throw by keeping this thread blocked.
                // See lock_failed::try_again
                bool try_again = l.try_lock_shared_for (shared_state_chrono::duration<double>{timeout});
                if (try_again)
                    l.unlock_shared ();

                d->template timeout_failed<T> (timeout, try_again);
                // timeout_failed is expected to throw. But if it doesn't,
                // make this behave as a null pointer
                p.reset ();
            }

            d->was_locked();
        }

        shared_state_mutex& l;
        // p is 'const T', compared to shared_state::p which is just 'T'.
        std::shared_ptr<const T> p;
        std::shared_ptr<details> d;
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
            :   l (b.l)
        {
            p.swap (b.p);
            d.swap (b.d);
        }

        write_ptr(const write_ptr&) = delete;
        write_ptr& operator=(write_ptr const&) = delete;

        ~write_ptr() {
            unlock ();
        }

        T* operator-> () const { return p.get (); }
        T& operator* () const { return *p; }
        T* get () const { return p.get (); }
        explicit operator bool() const { return (bool)p; }

        void unlock() {
            if (p)
            {
                l.unlock ();
                d->was_unlocked();
            }

            p.reset ();
        }

    private:
        friend class shared_state;

        template<class = typename disable_if <std::is_convertible<const element_type*, element_type*>::value>::type>
        explicit write_ptr (const shared_state& vp)
            :   l (vp.readWriteLock()),
                p (vp.p),
                d (vp.d)
        {
            lock ();
        }

        template<class = typename disable_if <std::is_convertible<const element_type*, element_type*>::value>::type>
        write_ptr (const shared_state& vp, bool)
            :   l (vp.readWriteLock()),
                p (vp.p),
                d (vp.d)
        {
            if (!l.try_lock ())
                p.reset ();
            else
                d->was_locked();
        }

        // See read_ptr::lock
        void lock() {
            double timeout = d->timeout();

            if (l.try_lock())
            {
            }
            else if (timeout < 0)
            {
                l.lock ();
            }
            else if (l.try_lock_for (shared_state_chrono::duration<double>{timeout}))
            {
            }
            else
            {
                bool try_again = l.try_lock_for (shared_state_chrono::duration<double>{timeout});
                if (try_again)
                    l.unlock ();

                d->template timeout_failed<T> (timeout, try_again);
                // timeout_failed is expected to throw. But if it doesn't,
                // make this behave as a null pointer
                p.reset ();
            }

            d->was_locked();
        }

        shared_state_mutex& l;
        std::shared_ptr<T> p;
        std::shared_ptr<details> d;
    };


    /**
     * @brief read provides thread safe read-only access.
     */
    read_ptr read() const { return read_ptr(*this); }

    /**
     * @brief write provides thread safe read and write access. Not accessible
     * if T is const.
     */
    write_ptr write() { return write_ptr(*this); }

    /**
     * @brief try_read obtains the lock only if it is readily available.
     *
     * If the lock was not obtained it doesn't throw any exception, but the
     * accessors return null pointers. This function fails much faster (about
     * 30x faster) than setting timeout=0 and discarding any lock_failed.
     */
    read_ptr try_read() const { return read_ptr(*this, bool()); }

    /**
     * @brief try_write. See try_read.
     */
    write_ptr try_write() { return write_ptr(*this, bool()); }

    /**
     * @brief readWriteLock returns the mutex object for this instance.
     */
    shared_state_mutex& readWriteLock() const { return d->lock; }

    /**
     * @brief unprotected gives direct access to the unprotected state for
     * using other synchornization mechanisms. Consider using read() or write()
     * instead.
     */
    std::shared_ptr<T> unprotected() { return p; }
    std::shared_ptr<const T> unprotected() const { return p; }

    /**
     * @brief details provides unprotected access to the instance of
     * shared_state_traits used for this type.
     */
    std::shared_ptr<typename shared_state_traits_helper<T>::type> traits() const { return d; }

private:
    template<typename Y>
    friend class shared_state;

    shared_state ( std::shared_ptr<T> p, std::shared_ptr<details> d ) : p(p), d(d) {}

    std::shared_ptr<T> p;
    std::shared_ptr<details> d;
};

class shared_state_test {
public:
    static void test ();
};

#endif // SHARED_STATE_H
