#ifndef UNDER_WIN32
#ifndef UNDER_IOT
#define UNDER_MRE
#endif
#endif

#ifdef UNDER_MRE
#include "vmlog.h"
#include "vmstdlib.h"
#endif


#ifdef _WIN32
//#define ENABLE_MEM_CHECK
//#define ENABLE_MEM_PROFILE
#endif

#include "string.h"
#include "stdlib.h"
#include "stdio.h"
#include "stdarg.h"
#include "ctype.h"

#ifdef  UNDER_WIN32
#include "windows.h"
#endif

/* Define to empty if the keyword does not work.  */
//#undef const

/* Define as __inline if that's what the C compiler calls it.  */
#define inline


/* The number of bytes in a char.  */
#define SIZEOF_CHAR 1

/* The number of bytes in a int.  */
#define SIZEOF_INT 4

/* The number of bytes in a long.  */
#define SIZEOF_LONG 4

/* The number of bytes in a long long.  */
#define SIZEOF_LONG_LONG 8

/* The number of bytes in a short.  */
#define SIZEOF_SHORT 2

/* The number of bytes in a void *.  */
#define SIZEOF_VOID_P 4

/* Define if you have the memmove function.  */
#undef HAVE_MEMMOVE

/* Define if you have the strcasecmp function.  */
#undef HAVE_STRCASECMP

#ifdef UNDER_MRE

void g_log_init(char* log_file);
int g_log_enabled(char* file, int line, char* fun, char* level);
void g_log(char* fmt, ...);

#define g_tcp_read vm_tcp_read
#define g_tcp_write vm_tcp_write
#define g_tcp_close vm_tcp_close
#define g_create_timer vm_create_timer
#define g_create_timer_ex vm_create_timer_ex

#define GDateTime vm_time_t
#define g_get_time vm_get_time

#define g_warning g_log_warn
#define g_error g_log_error

#ifdef _WIN32

#define g_log_fatal if (g_log_enabled(__FILE__, __LINE__, __FUNCTION__, "FATAL")) g_log
#define g_log_error if (g_log_enabled(__FILE__, __LINE__, __FUNCTION__, "ERROR")) g_log
#define g_log_warn if (g_log_enabled(__FILE__, __LINE__, __FUNCTION__, "WARN")) g_log
#define g_log_info if (g_log_enabled(__FILE__, __LINE__, __FUNCTION__, "INFO")) g_log
#define g_log_debug if (g_log_enabled(__FILE__, __LINE__, __FUNCTION__, "DEBUG")) g_log

#else

#define g_log_fatal if (g_log_enabled(__FILE__, __LINE__, "", "FATAL")) g_log
#define g_log_error if (g_log_enabled(__FILE__, __LINE__, "", "ERROR")) g_log
#define g_log_warn if (g_log_enabled(__FILE__, __LINE__, "", "WARN")) g_log
#define g_log_info if (g_log_enabled(__FILE__, __LINE__, "", "INFO")) g_log
#define g_log_debug if (g_log_enabled(__FILE__, __LINE__, "", "DEBUG")) g_log

#endif 


#define g_get_tick_count vm_get_tick_count

#define malloc vm_malloc
#define calloc(size, n) vm_calloc((size) * (n))
#define realloc vm_realloc
#define free vm_free
#endif

#ifdef UNDER_WIN32

#define g_get_tick_count GetTickCount
#endif

#ifndef UNDER_MRE

#define g_warning g_log_warn
#define g_error g_log_error

#define g_log_fatal printf
#define g_log_error printf
#define g_log_warn printf
#define g_log_info printf
#define g_log_debug printf

#endif

#ifdef _WIN32
#define g_print printf
#else
#define g_print 
#endif
