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
	// try/catch needed here to prevent inlining in 32-bit builds on 64-bit windows (WOW64)
	try {
		throw_catchable_segfault_exception2();
	} catch(...) {
		throw;
	}
}

void throw_catchable_segfault_exception3_noinline()
{
	try {
	    throw_catchable_segfault_exception3();
	} catch(...) {
		throw;
	}
}

void throw_catchable_segfault_exception4_noinline()
{
	try {
	    throw_catchable_segfault_exception4();
	} catch(...) {
		throw;
	}
}

void throw_catchable_segfault_exception5_noinline()
{
	try {
		throw_catchable_segfault_exception5();
	} catch(...) {
		throw;
	}
}
