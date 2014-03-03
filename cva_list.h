#pragma once

#include <stdarg.h>

/**
	Please note, this class is not at all more safe than to use the
	va_list macros directly, it's just a slightly more convenient way
	to first start and then subsequently end a va_list.

	And please do keep in mind the comment above, just as with va_arg,
	the only possible error indication you'd ever get from this class 
	is a crash because of memory access violation (unless you're using
	clr:pure).
*/

#define Cva_start(name, argument) Cva_list name; va_start((va_list)name,argument);

class Cva_list
{
public:
#ifdef _MSC_VER
        Cva_list(va_list cheatlist):list(cheatlist) {}
#else
        Cva_list(va_list cheatlist) { va_copy(list, cheatlist);}
#endif
        Cva_list()
        {
        }

	~Cva_list( )
	{
	}

	template<typename type>
	type& cva_arg()
	{
		return va_arg( list, type );
	}

	operator va_list&(){ return list; }
private:
        va_list list;
};
