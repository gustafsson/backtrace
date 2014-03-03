#ifndef BACKTRACE_H
#define BACKTRACE_H

#include <vector>
#include <string>

#include <boost/exception/all.hpp>

/**
 * @brief The Backtrace class should store a backtrace of the call stack in 1 ms,
 * except for windows where it should takes 30 ms but include a backtrace from all threads.
 *
 * It should work as error info to boost::exception
 *
 * It should translate to a pretty backtrace when asked for a string representation.
 *
 * Include debug info '-g' for this to work.
 */
class Backtrace
    {
    public:
        typedef boost::error_info<struct tag,Backtrace> info;

        static info make(int skipframes=1);
        static std::string make_string(int skipframes=1);
        static void malloc_free_log();
        std::string to_string();
        std::string to_string() const;

    private:
        Backtrace();

        std::string pretty_print_;
        std::vector<void*> frames_;

    public:
        static void test();
    };


inline std::string to_string( Backtrace::info const & x )
    {
    return x.value().to_string();
    }


inline std::string to_string( Backtrace::info & x )
    {
    return x.value().to_string();
    }


// This is a bad idea because it takes a lot of time.
//#define BACKTRACE()
//    (str(boost::format("%s:%d %s\n%s") % __FILE__ % __LINE__ % BOOST_CURRENT_FUNCTION % Backtrace::make_string()))


#endif // BACKTRACE_H
