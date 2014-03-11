# The intentaion is not to build these snippets as a .lib, just include them in your source.
# This serves to run a unit test.


CXX           = clang++
LINK          = clang++


BACKTRACE_CXXFLAGS = -fno-omit-frame-pointer
BACKTRACE_LFLAGS   = -rdynamic
# no-omit-frame-pointer and -rdynamic aren't strictly needed for backtrace to work
# but it makes info in the backtrace more likely to correspond to your code.


# pick one
#DEBUG_RELEASE = -D_DEBUG
DEBUG_RELEASE = -O3


INCPATH       = -I/opt/local/include
CXXFLAGS      = -std=c++11 -Wall -g $(BACKTRACE_CXXFLAGS) $(DEBUG_RELEASE) $(INCPATH)
LFLAGS        = $(BACKTRACE_LFLAGS)
LIBS          = -L/opt/local/lib -lboost_chrono-mt -lboost_system-mt -lboost_thread-mt -lboost_filesystem-mt


TARGET        = ./backtrace-unittest
OBJECTS       = \
		backtrace.o \
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
