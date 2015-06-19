#ifndef DEMANGLE_H
#define DEMANGLE_H

#include <string>
#include <typeinfo>

/**
 * Demangle should perform a system specific demangling of compiled C++ names.
 */

std::string demangle(const std::type_info& i);
std::string demangle(const char* d);

template<typename T>
std::string vartype(T const& t) { 
    // suppress warning: unreferenced formal parameter
    // even though 't' is used by typeid below
    //&t?void():void();

    // Note that typeid(t) might differ from typeid(T) as T is deduced
    // in compile time and t in runtime with RTTI.
    return demangle(typeid(t));
}

std::ostream& operator<< (std::ostream& o, const std::type_info& i);

#endif // DEMANGLE_H

