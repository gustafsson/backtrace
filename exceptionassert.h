#ifndef EXCEPTIONASSERT_H
#define EXCEPTIONASSERT_H

#include <string>
#include <stdexcept>
#include <boost/format.hpp>
#include <boost/exception/all.hpp>

typedef boost::error_info<struct errinfo_format_tag, boost::format> errinfo_format;


/**
 * @brief The ExceptionAssert class should store details about an assertion
 * that failed.
 *
 * Use like so
 *
 * try {
 *     EXCEPTION_ASSERT_EQUALS( 1, 2 );
 * } catch (const exception& x) {
 *     string what = boost::diagnostic_information(x);
 * }
 */
class ExceptionAssert: virtual public boost::exception, virtual public std::exception
{
public:
    typedef boost::error_info<struct condition,char const *> ExceptionAssert_condition;
    typedef boost::error_info<struct message,std::string> ExceptionAssert_message;

    // Prevent inlining of creating exception object
    static void throwException(const char* functionMacro,
                        const char* fileMacro, int lineMacro,
                        const char* details,
                        const std::string&, int skipFrames=0);

    static void throwException(const char* functionMacro,
                        const char* fileMacro, int lineMacro,
                        const char* details,
                        const boost::format&);

    static void logAndThrow(const char* functionMacro,
                        const char* fileMacro, int lineMacro,
                        const char* details,
                        const std::string&, int skipFrames=0);

    static void logAndThrow(const char* functionMacro,
                        const char* fileMacro, int lineMacro,
                        const char* details,
                        const boost::format&);

    static void logError(const char* functionMacro,
                         const char* fileMacro, int lineMacro,
                         const char* condition,
                         const std::string&);

    static void logError(const char* functionMacro,
                         const char* fileMacro, int lineMacro,
                         const char* condition,
                         const boost::format&);

private:
    ExceptionAssert();

public:
    static void test();
};

#define LOG_ERROR( msg ) \
    do { \
        ExceptionAssert::logError( BOOST_CURRENT_FUNCTION, __FILE__, __LINE__, "LOG_ERROR", msg ); \
    } while(false)

// 'condition' is evaluated once, 'msg' is a boost::format, std::string or compatible type
#define EXCEPTION_ASSERTX( condition, msg ) \
    do { \
        if (!(condition)) \
            ExceptionAssert::throwException( BOOST_CURRENT_FUNCTION, __FILE__, __LINE__, #condition, msg ); \
    } while(false)

#define EXCEPTION_ASSERT( condition ) EXCEPTION_ASSERTX( condition, "error" )

#define EXCEPTION_ASSERT_EQUALS( a, b ) \
    do { \
        const auto& _x = (a); \
        const auto& _y = (b); \
        EXCEPTION_ASSERTX( _x == _y, boost::format("Equals failed. Got '%1%' = %3%, and '%2%' = %4%") % #a % #b % _x % _y); \
    } while(false)

#define EXCEPTION_ASSERT_NOTEQUALS( a, b ) \
    do { \
        const auto& _x = (a); \
        const auto& _y = (b); \
        EXCEPTION_ASSERTX( _x != _y, boost::format("Not equals failed. Got '%1%' = %3%, and '%2%' = %4%") % #a % #b % _x % _y); \
    } while(false)

#define EXCEPTION_ASSERT_FUZZYEQUALS( a, b, d ) \
    do { \
        const auto& _x = (a); \
        const auto& _y = (b); \
        const auto& _z = (d); \
        EXCEPTION_ASSERTX( (_x > _y ? _x - _y <= _z : _y - _x <= _z), boost::format("Fuzzy equals failed. Got '%1%' = %3%, and '%2%' = %4%, with diff = %6%, tolerance = %5%") % #a % #b % _x % _y % _z % (_x > _y ? _x - _y : _y - _x)); \
    } while(false)

#define EXCEPTION_ASSERT_LESS( a, b ) \
    do { \
        const auto& _x = (a); \
        const auto& _y = (b); \
        EXCEPTION_ASSERTX( _x < _y, boost::format("Less failed. Got '%1%' = %3%, and '%2%' = %4%") % #a % #b % _x % _y); \
    } while(false)

#define EXCEPTION_ASSERT_LESS_OR_EQUAL( a, b ) \
    do { \
        const auto& _x = (a); \
        const auto& _y = (b); \
        EXCEPTION_ASSERTX( _x <= _y, boost::format("Less or equal failed. Got '%1%' = %3%, and '%2%' = %4%") % #a % #b % _x % _y); \
    } while(false)

#ifdef _DEBUG
#define EXCEPTION_ASSERTX_DBG EXCEPTION_ASSERTX
#define EXCEPTION_ASSERT_DBG EXCEPTION_ASSERT
#define EXCEPTION_ASSERT_EQUALS_DBG EXCEPTION_ASSERT_EQUALS
#define EXCEPTION_ASSERT_NOTEQUALS_DBG EXCEPTION_ASSERT_NOTEQUALS
#define EXCEPTION_ASSERT_FUZZYEQUALS_DBG EXCEPTION_ASSERT_FUZZYEQUALS
#define EXCEPTION_ASSERT_LESS_DBG EXCEPTION_ASSERT_LESS
#else
#define EXCEPTION_ASSERTX_DBG( condition, msg )
#define EXCEPTION_ASSERT_DBG( condition )
#define EXCEPTION_ASSERT_EQUALS_DBG( a, b )
#define EXCEPTION_ASSERT_NOTEQUALS_DBG( a, b )
#define EXCEPTION_ASSERT_FUZZYEQUALS_DBG( a, b, c )
#define EXCEPTION_ASSERT_LESS_DBG( a, b )
#endif

#endif // EXCEPTIONASSERT_H
