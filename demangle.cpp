#include "demangle.h"
#include <stdlib.h>
#include <iostream>

using namespace std;

#ifdef __GNUC__

#include <cxxabi.h>
string demangle(const char* d) {
    int     status;
    char * a = abi::__cxa_demangle(d, 0, 0, &status);
	if (a) {
		string s(a);
        free(a);
		return s;
	}
    return string(d);
}

#elif defined(_MSC_VER)

extern "C"
char * __unDName(
	char * outputString,
	const char * name,
	int maxStringLength,
	void * (__cdecl * pAlloc )(size_t),
	void (__cdecl * pFree )(void *),
	unsigned short disableFlags);

string demangle(const char* d) {
	char * const pTmpUndName = __unDName(0, d, 0, malloc, free, 0x2800);
	if (pTmpUndName)
	{
		string s(pTmpUndName);
		free(pTmpUndName);
		return s;
	}
    return string(d);
}

#else

// TODO find solution for this compiler
std::string demangle(const char* d) {
	return string(d);
}

std::string demangle(const char* d) {
    int pointer = 0;
    string s;
    while ('P' == *d) { pointer++; d++; }
    if ('f' == *d) { s += "float"; }
    if ('d' == *d) { s += "double"; }
    int i = atoi(d);
    if (i>0) {
        d++;
        while (i>0) {
            s+=*d;
            i--;
            d++;
        }
    }
    if (s.empty())
        s+=d;
    if ('I' == *d) {
        d++;
        s+="<";
        s+=demangle(d);
        s+=">";
        if (0==pointer)
            s+=" ";
    }

    while (pointer>0) {
        s+="*";
        pointer--;
    }

    return s;
}

#endif


std::string demangle(const std::type_info& i)
{
    return demangle(i.name());
}


std::ostream& operator<< (std::ostream& o, const std::type_info& i)
{
    return o << demangle(i);
}
