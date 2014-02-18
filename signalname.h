#ifndef SIGNALNAME_H
#define SIGNALNAME_H

/**
 * @brief The SignalName class should translate a system signal to a short and long string representation.
 */
class SignalName
{
public:
    static const char* name(int signal);
    static const char* desc(int signal);
};

#endif // SIGNALNAME_H
