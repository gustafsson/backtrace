#include <stdexcept>

void throw_catchable_segfault_exception();
void throw_catchable_segfault_exception2();
void throw_catchable_segfault_exception3();
void throw_catchable_segfault_exception4();
void throw_catchable_segfault_exception5();

// Placing a function in a different compilation unit usually prevents inlining

void throw_catchable_segfault_exception_noinline()
{
    throw_catchable_segfault_exception();
}

void throw_catchable_segfault_exception2_noinline()
{
    throw_catchable_segfault_exception2();
}

void throw_catchable_segfault_exception3_noinline()
{
    throw_catchable_segfault_exception3();
}

void throw_catchable_segfault_exception4_noinline()
{
    throw_catchable_segfault_exception4();
}

void throw_catchable_segfault_exception5_noinline()
{
    throw_catchable_segfault_exception5();
}
