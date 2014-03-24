#ifndef SHARED_STATE_MUTEX_H
#define SHARED_STATE_MUTEX_H

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

#endif // SHARED_STATE_MUTEX_H
