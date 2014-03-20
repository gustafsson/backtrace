#ifndef TIMER_H
#define TIMER_H

#ifndef _MSC_VER
#include <chrono>
#endif

/**
 * @brief The Timer class should measure duration with a high accuracy.
 *
 * It should have an overhead less than 1 microsecond.
 */
class Timer
{
public:
    Timer(bool start=true);

    void restart();
    double elapsed() const;
    double elapsedAndRestart();

private:
#ifdef _MSC_VER
    __int64 start_;
#else
    std::chrono::high_resolution_clock::time_point start_;
#endif

public:
    static void test();
};

#endif // TIMER_H
