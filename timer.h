#ifndef TIMER_H
#define TIMER_H

#ifndef _MSC_VER
#include <boost/date_time/posix_time/posix_time.hpp>
#endif

/**
 * @brief The Timer class should measure time with a high accuracy
 * (at least 0.1-millisecond resolution).
 */
class Timer
{
public:
    Timer();

    void restart();
    double elapsed() const;
    double elapsedAndRestart();

private:
#ifdef _MSC_VER
    __int64 start_;
#else
    boost::posix_time::ptime start_;
#endif

public:
    static void test();
};

#endif // TIMER_H
