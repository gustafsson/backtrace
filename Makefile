# The intentaion is not to build these snippets as a .lib, just include them in your source.
# This serves to run a unit test.


CXX           = clang++
LINK          = clang++


BACKTRACE_CXXFLAGS = -fno-omit-frame-pointer
BACKTRACE_LFLAGS   = -rdynamic
# no-omit-frame-pointer and -rdynamic aren't strictly needed for backtrace to work
# but it makes info in the backtrace more likely to correspond to your code.


# pick one, release or debug
#DEBUG_RELEASE = -D_DEBUG
DEBUG_RELEASE = -O3


# shared_state configuration
#
# shared_state supports concurrent reads by default. The overhead of enabling
# concurrent reads is about 5% larger than when concurernt reads are disabled
# if there is no lock contention. The overhead is about 0.8 microseconds.
#
#SHARED_STATE  = -DSHARED_STATE_NO_SHARED_MUTEX
#SHARED_STATE  = -DSHARED_STATE_NO_TIMEOUT
#SHARED_STATE  = -DSHARED_STATE_NO_SHARED_MUTEX -DSHARED_STATE_NO_TIMEOUT
#
# std (libc++) is 20% faster than boost with concurrent reads enabled.
# boost (pthreads) is 1% faster than std without concurrent reads.
#
#LIBS          = -L/opt/local/lib -lboost_system-mt -lboost_chrono-mt -lboost_thread-mt
#SHARED_STATE  = -DSHARED_STATE_BOOST_MUTEX
#SHARED_STATE  = -DSHARED_STATE_BOOST_MUTEX -DSHARED_STATE_NO_SHARED_MUTEX
#SHARED_STATE  = -DSHARED_STATE_BOOST_MUTEX -DSHARED_STATE_NO_TIMEOUT
#SHARED_STATE  = -DSHARED_STATE_BOOST_MUTEX -DSHARED_STATE_NO_SHARED_MUTEX -DSHARED_STATE_NO_TIMEOUT


INCPATH       = -I/opt/local/include
CXXFLAGS      = -std=c++11 -W -Wall -g $(BACKTRACE_CXXFLAGS) $(DEBUG_RELEASE) $(SHARED_STATE) $(INCPATH)
LFLAGS        = $(BACKTRACE_LFLAGS)


TARGET        = ./backtrace-unittest
OBJECTS       = \
		backtrace.o \
		barrier.o \
		demangle.o \
		detectgdb.o \
		exceptionassert.o \
		main.o \
		prettifysegfault.o \
		prettifysegfaultnoinline.o \
		signalname.o \
		tasktimer.o \
		timer.o \
		trace_perf.o \
		unittest.o \
		verifyexecutiontime.o \
		shared_state.o \

all: $(TARGET)

clean:
	rm -f $(OBJECTS) $(TARGET)

.depend: *.cpp *.h
	mkdep $(CXXFLAGS) *.cpp

$(TARGET): Makefile $(OBJECTS) .depend
	$(LINK) $(LFLAGS) -o $(TARGET) $(OBJECTS) $(LIBS)
	$(TARGET) || true

include .depend
