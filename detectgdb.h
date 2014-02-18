#ifndef DETECTGDB_H
#define DETECTGDB_H

/**
 * @brief The DetectGdb class should detect whether the current process was
 * started through, or is running through, gdb.
 *
 * It might work with lldb or other debuggers as well.
 */
class DetectGdb
{
public:
    static bool is_running_through_gdb();
    static bool was_started_through_gdb();
};

#endif // DETECTGDB_H
