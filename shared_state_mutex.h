#ifndef SHARED_STATE_MUTEX_H
#define SHARED_STATE_MUTEX_H

#ifdef SHARED_STATE_BOOST_MUTEX
    #include <boost/thread/shared_mutex.hpp>

    namespace shared_state_chrono = boost::chrono;

    class shared_state_mutex_notimeout_noshared: public boost::mutex {
    public:
        // recursive read locks are allowed to dead-lock so it is valid to replace a shared_timed_mutex with a mutex
        void lock_shared() { lock(); }
        bool try_lock_shared() { return try_lock(); }
        void unlock_shared() { unlock(); }

        // Discard any timeout parameters
        bool try_lock_for(...) { lock(); return true; }
        bool try_lock_shared_for(...) { lock_shared(); return true; }
    };

    class shared_state_mutex_notimeout: public boost::shared_mutex {
    public:
        // Discard any timeout parameters
        bool try_lock_for(...) { lock(); return true; }
        bool try_lock_shared_for(...) { lock_shared(); return true; }
    };

    class shared_state_mutex_noshared: public boost::timed_mutex {
    public:
        void lock_shared() { lock(); }
        bool try_lock_shared() { return try_lock(); }
        void unlock_shared() { unlock(); }

        template <class Rep, class Period>
        bool try_lock_shared_for(const shared_state_chrono::duration<Rep, Period>& rel_time) { return try_lock_for(rel_time); }
    };

    typedef boost::shared_mutex shared_state_mutex_default;
#else
    //#include <mutex>
    //#include <shared_mutex>  // Requires C++14
    //namespace std {
    //    typedef shared_mutex shared_timed_mutex;
    //}

    #include "shared_timed_mutex_polyfill.h" // Requires C++11
    namespace std {
        using namespace std_polyfill;
    }

    namespace shared_state_chrono = std::chrono;

    class shared_state_mutex_notimeout_noshared: public std::mutex {
    public:
        void lock_shared() { lock(); }
        bool try_lock_shared() { return try_lock(); }
        void unlock_shared() { unlock(); }

        bool try_lock_for(...) { lock(); return true; }
        bool try_lock_shared_for(...) { lock_shared(); return true; }
    };

    class shared_state_mutex_notimeout: public std::shared_timed_mutex {
    public:
        bool try_lock_for(...) { lock(); return true; }
        bool try_lock_shared_for(...) { lock_shared(); return true; }
    };

    class shared_state_mutex_noshared: public std::timed_mutex {
    public:
        void lock_shared() { lock(); }
        bool try_lock_shared() { return try_lock(); }
        void unlock_shared() { unlock(); }

        template <class Rep, class Period>
        bool try_lock_shared_for(const shared_state_chrono::duration<Rep, Period>& rel_time) { return try_lock_for(rel_time); }
    };

    typedef std::shared_timed_mutex shared_state_mutex_default;
#endif


#if defined SHARED_STATE_NO_TIMEOUT
    #if defined SHARED_STATE_NO_SHARED_MUTEX
        typedef shared_state_mutex_notimeout_noshared shared_state_mutex;
    #else
        typedef shared_state_mutex_notimeout shared_state_mutex;
    #endif
#elif defined SHARED_STATE_NO_SHARED_MUTEX
    typedef shared_state_mutex_noshared shared_state_mutex;
#else
    typedef shared_state_mutex_default shared_state_mutex;
#endif


#endif // SHARED_STATE_MUTEX_H
