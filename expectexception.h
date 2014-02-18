#ifndef EXPECTEXCEPTION_H
#define EXPECTEXCEPTION_H

#include "backtrace.h"
#include "demangle.h"

/**
 * The EXPECT_EXCEPTION macro should ensure that an expression throws an exception of a given type.
 */

class exptected_exception: virtual public boost::exception, virtual public std::exception {};
class unexptected_exception: virtual public boost::exception, virtual public std::exception {};
typedef boost::error_info<struct unexpected_exception_tag,boost::exception_ptr> unexpected_exception_info;

typedef boost::error_info<struct tag_expected_exception_type,std::type_info const *> expected_exception_type;

inline
std::string
to_string( expected_exception_type const & x )
    {
    return demangle(*x.value());
    }

#define EXPECT_EXCEPTION(X,F) do { \
    bool thrown = false; \
                         \
    try {                \
        F;               \
    } catch (const X&) { \
        thrown = true;   \
    } catch (const std::exception& x) { \
        BOOST_THROW_EXCEPTION(unexptected_exception()  \
            << unexpected_exception_info(boost::current_exception ()) \
            << Backtrace::make () \
            << expected_exception_type(&typeid(X))); \
    }                    \
                         \
    if (!thrown)         \
        BOOST_THROW_EXCEPTION(exptected_exception() \
            << Backtrace::make () \
            << expected_exception_type(&typeid(X))); \
} while(false);


#endif // EXPECTEXCEPTION_H
