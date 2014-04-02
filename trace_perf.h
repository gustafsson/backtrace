#ifndef TRACE_PERF_H
#define TRACE_PERF_H

#include <string>
#include "timer.h"

/**
 * @brief The trace_perf class should log the execution time of a scope and
 * warn if it was below a threshold.
 *
 * The scope is identified by a text string.
 *
 * The threshold is defined for each scope in a database file in the folder
 * trace_perf/...
 *
 * Multiple database files can be used to overload the thresholds.
 *
 * The results stored in a complementary database file regardless of failure
 * or success when the process quits.
 */
class trace_perf
{
public:
    trace_perf(const char* filename, const std::string& info);
    trace_perf(const trace_perf&) = delete;
    trace_perf& operator=(const trace_perf&) = delete;
    ~trace_perf();

    void reset();
    void reset(const std::string& info);

    static void add_database_path(const std::string& path);
private:
    Timer timer;
    std::string info;
    std::string filename;
};

#define TRACE_PERF(info) trace_perf trace_perf_{__FILE__, info}

#endif // TRACE_PERF_H
