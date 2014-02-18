#ifndef PRETTIFYSEGFAULT_H
#define PRETTIFYSEGFAULT_H

#include <boost/exception/all.hpp>

/**
 * @brief The PrettifySegfault class should attempt to capture any null-pointer
 * exception (SIGSEGV and SIGILL) in the program, log a backtrace, and then
 * throw a regular C++ exception from the function causing the signal.
 *
 *     When you attempt to recover from segfaults,
 *     you are playing with fire.
 *
 *     Once a segfault has been detected without a crash,
 *     you should restart the process. It is likely that a segfault
 *     will still crash the process, but there should at least be
 *     some info in the log.
 *
 * Throwing from within a signal handler is undefined behavior. This
 * implementation is based on a hack to rewrite the function stack. Please
 * refer to 'feepingcreature' for the full explanation at
 * http://feepingcreature.github.io/handling.html
 *
 * !!!
 * However, this doesn't always work. See note number 4 at the url above. And
 * it returns immediately from the signalling function without unwinding
 * the scope and thus break the RAII assumption that a destructor will
 * always be called. I.e leaking resources and potentially leaving the
 * process in an unconsistent state (and this is in addition to any harm that
 * happened before the original SIGSEGV/SIGILL was detected).
 * !!!
 * So don't rely on this class, it's not a safety net, it merely serves to
 * quickly indicate the location of a severe error when it occurs.
 * !!!
 *
 * While at it, PrettifySegfault::setup() sets up logging of all other signal
 * types as well if they are detected, but without taking any further action.
 * (except SIGCHLD which is ignored)
 */
class PrettifySegfault
{
public:
    enum SignalHandlingState {
        normal_execution,
        doing_signal_handling
    };

    /**
     * @brief setup enables PrettifySegfault.
     */
    static void setup();

    /**
     * @return If the process is in the state of signal handling you
     * should proceed to exit the process.
     */
    static SignalHandlingState signal_handling_state ();
    static bool has_caught_any_signal ();

    /**
     * @brief PrettifySegfaultDirectPrint makes the signal handler write info
     * to stdout as soon as the signal is caught. Default enable=true.
     * @param enable
     */
    static void EnableDirectPrint(bool enable);

    /**
     * @brief test will cause a segfault. This will put the process in
     * signal_handling_state and prevent further signals from being caught.
     * Such signals will instead halt the process.
     */
    static void test();
};


class signal_exception: virtual public boost::exception, virtual public std::exception {
public:
    typedef boost::error_info<struct signal, int> signal;
    typedef boost::error_info<struct signalname, const char*> signalname;
    typedef boost::error_info<struct signaldesc, const char*> signaldesc;
};
class segfault_sigill_exception: public signal_exception {};

#endif // PRETTIFYSEGFAULT_H
