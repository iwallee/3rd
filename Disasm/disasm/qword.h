#ifndef LIBDISASM_QWORD_H
#define LIBDISASM_QWORD_H

/* platform independent data types */

#ifdef _MSC_VER
	typedef __int64         qword_t;
#else
	typedef long long       qword_t;
#endif

#endif
