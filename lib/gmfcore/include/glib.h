#ifndef __G_LIB_H__
#define __G_LIB_H__

#include "glibconfig.h"

#ifdef USE_DMALLOC
#include "dmalloc.h"
#endif

/* Provide definitions for some commonly used macros.
 *  Some of them are only provided if they haven't already
 *  been defined. It is assumed that if they are already
 *  defined then the current definition is correct.
 */
#ifndef	NULL
#define	NULL	((void*) 0)
#endif

#ifndef	FALSE
#define	FALSE	(0)
#endif

#ifndef	TRUE
#define	TRUE	(!FALSE)
#endif

#undef	MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))

#undef	MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))

#undef	ABS
#define ABS(a)	   (((a) < 0) ? -(a) : (a))

#undef	CLAMP
#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

#undef	MAXUINT
#define MAXUINT     ((guint)~((guint)0))

#undef	MAXINT
#define MAXINT      ((gint)(MAXUINT >> 1))

#undef	MININT
#define MININT      ((gint)~MAXINT)

/* Provide simple macro statement wrappers (adapted from Perl):
 *  G_STMT_START { statements; } G_STMT_END;
 *  can be used as a single statement, as in
 *  if (x) G_STMT_START { ... } G_STMT_END; else ...
 *
 *  For gcc we will wrap the statements within `({' and `})' braces.
 *  For SunOS they will be wrapped within `if (1)' and `else (void) 0',
 *  and otherwise within `do' and `while (0)'.
 */
#if !(defined (G_STMT_START) && defined (G_STMT_END))
#  if defined (__GNUC__) && !defined (__STRICT_ANSI__) && !defined (__cplusplus)
#    define G_STMT_START	(void)(
#    define G_STMT_END		)
#  else
#    if (defined (sun) || defined (__sun__))
#      define G_STMT_START	if (1)
#      define G_STMT_END	else (void)0
#    else
#      define G_STMT_START	do
#      define G_STMT_END	while (0)
#    endif
#  endif
#endif

/* Provide macros for easily allocating memory. The macros
 *  will cast the allocated memory to the specified type
 *  in order to avoid compiler warnings. (Makes the code neater).
 */

#ifdef __DMALLOC_H__

#define g_new(type,count)	 ALLOC(type,count)
#define g_new0(type,count)	 CALLOC(type,count)

#else /* __DMALLOC_H__ */

#define g_new(type, count)	  \
    ((type *) g_malloc ((unsigned) sizeof (type) * (count)))
#define g_new0(type, count)	  \
    ((type *) g_malloc0 ((unsigned) sizeof (type) * (count)))
#endif /* __DMALLOC_H__ */

#define g_mem_chunk_create(type, pre_alloc, alloc_type)	( \
  g_mem_chunk_new (#type " mem chunks (" #pre_alloc ")", \
		   sizeof (type), \
		   sizeof (type) * (pre_alloc), \
		   (alloc_type)) \
)
#define g_chunk_new(type, chunk)	( \
  (type *) g_mem_chunk_alloc (chunk) \
)
#define g_chunk_new0(type, chunk)	( \
  (type *) memset (g_mem_chunk_alloc (chunk), 0, sizeof (type)) \
)
#define g_chunk_free(mem, mem_chunk)	G_STMT_START { \
  g_mem_chunk_free ((mem_chunk), (mem)); \
} G_STMT_END

#define g_string(x) #x


/* Provide macros for error handling. The "assert" macros will
 *  exit on failure. The "return" macros will exit the current
 *  function. Two different definitions are given for the macros
 *  in order to support gcc's __PRETTY_FUNCTION__ capability.
 */

#ifdef G_DISABLE_ASSERT

#define g_assert(expr)
#define g_assert_not_reached()

#else /* !G_DISABLE_ASSERT */

#ifdef __GNUC__

#define g_assert(expr)			G_STMT_START{\
     if (!(expr))				     \
       g_error ("file %s: line %d (%s): \"%s\"",     \
		__FILE__,			     \
		__LINE__,			     \
		__PRETTY_FUNCTION__,		     \
		#expr);			}G_STMT_END

#define g_assert_not_reached()		G_STMT_START{		      \
     g_error ("file %s: line %d (%s): \"should not be reached\"",     \
	      __FILE__,						      \
	      __LINE__,						      \
	      __PRETTY_FUNCTION__);	}G_STMT_END

#else /* !__GNUC__ */

#define g_assert(expr)			G_STMT_START{\
     if (!(expr))				     \
       g_error ("file %s: line %d: \"%s\"",	     \
		__FILE__,			     \
		__LINE__,			     \
		#expr);			}G_STMT_END

#define g_assert_not_reached()		G_STMT_START{		      \
     g_error ("file %s: line %d: \"should not be reached\"",	      \
	      __FILE__,						      \
	      __LINE__);		}G_STMT_END

#endif /* __GNUC__ */

#endif /* G_DISABLE_ASSERT */

#ifdef G_DISABLE_CHECKS

#define g_return_if_fail(expr)
#define g_return_val_if_fail(expr,val)

#else /* !G_DISABLE_CHECKS */

#ifdef __GNUC__

#define g_return_if_fail(expr)		G_STMT_START{		       \
     if (!(expr))						       \
       {							       \
	 g_warning ("file %s: line %d (%s): assertion \"%s\" failed.", \
		    __FILE__,					       \
		    __LINE__,					       \
		    __PRETTY_FUNCTION__,			       \
		    #expr);					       \
	 return;						       \
       };				}G_STMT_END

#define g_return_val_if_fail(expr,val)	G_STMT_START{		       \
     if (!(expr))						       \
       {							       \
	 g_warning ("file %s: line %d (%s): assertion \"%s\" failed.", \
		    __FILE__,					       \
		    __LINE__,					       \
		    __PRETTY_FUNCTION__,			       \
		    #expr);					       \
	 return val;						       \
       };				}G_STMT_END

#else /* !__GNUC__ */

#define g_return_if_fail(expr)		G_STMT_START{		  \
     if (!(expr))						  \
       {							  \
	 g_warning ("file %s: line %d: assertion. \"%s\" failed", \
		    __FILE__,					  \
		    __LINE__,					  \
		    #expr);					  \
	 return;						  \
       };				}G_STMT_END

#define g_return_val_if_fail(expr, val)	G_STMT_START{		  \
     if (!(expr))						  \
       {							  \
	 g_warning ("file %s: line %d: assertion \"%s\" failed.", \
		    __FILE__,					  \
		    __LINE__,					  \
		    #expr);					  \
	 return val;						  \
       };				}G_STMT_END

#endif /* !__GNUC__ */

#endif /* G_DISABLE_CHECKS */


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

/* Provide type definitions for commonly used types.
 *  These are useful because a "gint8" can be adjusted
 *  to be 1 byte (8 bits) on all platforms. Similarly and
 *  more importantly, "gint32" can be adjusted to be
 *  4 bytes (32 bits) on all platforms.
 */

typedef char   gchar;
typedef short  gshort;
typedef long   glong;
typedef int    gint;
typedef gint   gboolean;

typedef unsigned char	guchar;
typedef unsigned short	gushort;
typedef unsigned long	gulong;
typedef unsigned int	guint;

typedef float	gfloat;
typedef double	gdouble;

typedef void* gpointer;
typedef const void *gconstpointer;

typedef gpointer ghandle;

#if (SIZEOF_CHAR == 1)
typedef signed char	gint8;
typedef unsigned char	guint8;
#endif /* SIZEOF_CHAR */


#if (SIZEOF_SHORT == 2)
typedef signed short	gint16;
typedef unsigned short	guint16;
#endif /* SIZEOF_SHORT */


#if (SIZEOF_INT == 4)
typedef signed int	gint32;
typedef unsigned int	guint32;
#elif (SIZEOF_LONG == 4)
typedef signed long	gint32;
typedef unsigned long	guint32;
#endif /* SIZEOF_INT */

#if (SIZEOF_LONG == 8)
#define HAVE_GINT64 1
typedef signed long gint64;
typedef unsigned long guint64;
#elif (SIZEOF_LONG_LONG == 8)
#define HAVE_GINT64 1
typedef signed long long gint64;
typedef unsigned long long guint64;
#else
/* No gint64 */
#undef HAVE_GINT64
#endif

typedef guint16 gwchar;
typedef gchar * gstring;
typedef gwchar * gwstring;
typedef const gchar * gconststring;
typedef const gwchar * gconstwstring;

/* Define macros for storing integers inside pointers */

#if (SIZEOF_INT == SIZEOF_VOID_P)

#define GPOINTER_TO_INT(p) ((gint)(p))
#define GPOINTER_TO_UINT(p) ((guint)(p))

#define GINT_TO_POINTER(i) ((gpointer)(i))
#define GUINT_TO_POINTER(u) ((gpointer)(u))

#elif (SIZEOF_LONG == SIZEOF_VOID_P)

#define GPOINTER_TO_INT(p) ((gint)(glong)(p))
#define GPOINTER_TO_UINT(p) ((guint)(gulong)(p))

#define GINT_TO_POINTER(i) ((gpointer)(glong)(i))
#define GUINT_TO_POINTER(u) ((gpointer)(gulong)(u))

#else
/* This should never happen */
#endif

typedef struct _GFreeAtom   GFreeAtom;
typedef struct _GList		GList;
typedef struct _GSList		GSList;
typedef struct _GHashNode   GHashNode;
typedef struct _GHashTable	GHashTable;
typedef struct _GCache		GCache;
typedef struct _GTree		GTree;
typedef struct _GMemChunk	GMemChunk;
typedef struct _GListAllocator	GListAllocator;
typedef struct _GStringChunk	GStringChunk;
typedef struct _GString			GString;
typedef struct _GArray			GArray;
typedef struct _GPtrArray		GPtrArray;
typedef struct _GByteArray		GByteArray;
typedef struct _GTimer			GTimer;
typedef struct _GBuffer			GBuffer;

struct _GBuffer
{
	gint length;
	gpointer buffer;
};

struct _GTimer
{
	ghandle handle;
	gboolean enabled;
	gint interval;
	gboolean support_background_running;
	GList * callback_list;
	GList * data_list;
};

typedef void		(*GFunc)		(gpointer  data,
						 gpointer  user_data);
typedef void		(*GHFunc)		(gpointer  key,
						 gpointer  value,
						 gpointer  user_data);
typedef gpointer	(*GCacheNewFunc)	(gpointer  key);
typedef gpointer	(*GCacheDupFunc)	(gpointer  value);
typedef void		(*GCacheDestroyFunc)	(gpointer  value);
typedef gint		(*GTraverseFunc)	(gpointer  key,
						 gpointer  value,
						 gpointer  data);
typedef gint		(*GSearchFunc)		(gpointer  key,
						 gpointer  data);
typedef void		(*GErrorFunc)		(gchar	  *str);
typedef void		(*GWarningFunc)		(gchar	  *str);
typedef void		(*GPrintFunc)		(gchar	  *str);
typedef void		(*GDestroyNotify)	(gpointer  data);

typedef guint		(*GHashFunc)		(gconstpointer	  key);
typedef gint		(*GCompareFunc)		(gconstpointer	  a,
						 gconstpointer	  b);

typedef void		(*GTimerCallback)(GTimer * timer, gpointer data);

struct _GList
{
  gpointer data;
  GList *next;
  GList *prev;
};

struct _GSList
{
  gpointer data;
  GSList *next;
};

struct _GString
{
  gchar *str;
  gint len;
};

struct _GArray
{
  gchar *data;
  gint len;
};

struct _GByteArray
{
  guint8 *data;
  gint	  len;
};

struct _GPtrArray
{
  gpointer *pdata;
  gint	    len;
};

struct _GFreeAtom
{
	GFreeAtom *next;
};

struct _GHashNode
{
	guint hash;
	gpointer key;
	gpointer value;
};

struct _GHashTable
{
	GArray * nodes;
	GHashFunc hash_func;
	GCompareFunc key_compare_func;
};

struct _GCache { gint dummy; };
struct _GTree { gint dummy; };
struct _GMemChunk { gint dummy; };
struct _GListAllocator { gint dummy; };
struct _GStringChunk { gint dummy; };

typedef enum
{
  G_IN_ORDER,
  G_PRE_ORDER,
  G_POST_ORDER
} GTraverseType;

void g_buffer_set_length(gint len);
gint g_buffer_get_length(void);
gpointer g_buffer_get(void);
void g_buffer_release(gpointer buf);

/* Doubly linked lists
 */
GList* g_list_alloc		(void);
void   g_list_free		(GList		*list);
void   g_list_free_1		(GList		*list);
GList* g_list_append		(GList		*list,
				 gpointer	 data);
GList* g_list_prepend		(GList		*list,
				 gpointer	 data);
GList* g_list_insert		(GList		*list,
				 gpointer	 data,
				 gint		 position);
GList* g_list_insert_sorted	(GList		*list,
				 gpointer	 data,
				 GCompareFunc	 func);			    
GList* g_list_concat		(GList		*list1, 
				 GList		*list2);
GList* g_list_remove		(GList		*list,
				 gpointer	 data);
GList* g_list_remove_link	(GList		*list,
				 GList		*link);
GList* g_list_reverse		(GList		*list);
GList* g_list_nth		(GList		*list,
				 guint		 n);
GList* g_list_find		(GList		*list,
				 gpointer	 data);
GList* g_list_find_custom	(GList		*list,
				 gpointer	 data,
				 GCompareFunc	 func);
gint   g_list_position		(GList		*list,
				 GList		*link);
gint   g_list_index		(GList		*list,
				 gpointer	 data);
GList* g_list_last		(GList		*list);
GList* g_list_first		(GList		*list);
guint  g_list_length		(GList		*list);
void   g_list_foreach		(GList		*list,
				 GFunc		 func,
				 gpointer	 user_data);
gpointer g_list_nth_data	(GList		*list,
				 guint		 n);

#define g_list_previous(list)	((list) ? (((GList *)(list))->prev) : NULL)
#define g_list_next(list)	((list) ? (((GList *)(list))->next) : NULL)

/* Singly linked lists
 */
GSList* g_slist_alloc		(void);
void	g_slist_free		(GSList		*list);
void	g_slist_free_1		(GSList		*list);
GSList* g_slist_append		(GSList		*list,
				 gpointer	 data);
GSList* g_slist_prepend		(GSList		*list,
				 gpointer	 data);
GSList* g_slist_insert		(GSList		*list,
				 gpointer	 data,
				 gint		 position);
GSList* g_slist_insert_sorted	(GSList		*list,
				 gpointer	 data,
				 GCompareFunc	 func);			    
GSList* g_slist_concat		(GSList		*list1, 
				 GSList		*list2);
GSList* g_slist_remove		(GSList		*list,
				 gpointer	 data);
GSList* g_slist_remove_link	(GSList		*list,
				 GSList		*link);
GSList* g_slist_reverse		(GSList		*list);
GSList* g_slist_nth		(GSList		*list,
				 guint		 n);
GSList* g_slist_find		(GSList		*list,
				 gpointer	 data);
GSList* g_slist_find_custom	(GSList		*list,
				 gpointer	 data,
				 GCompareFunc	 func);
gint	g_slist_position	(GSList		*list,
				 GSList		*link);
gint	g_slist_index		(GSList		*list,
				 gpointer	 data);
GSList* g_slist_last		(GSList		*list);
guint	g_slist_length		(GSList		*list);
void	g_slist_foreach		(GSList		*list,
				 GFunc		 func,
				 gpointer	 user_data);
gpointer g_slist_nth_data	(GSList		*list,
				 guint		 n);

#define g_slist_next(slist)	((slist) ? (((GSList *)(slist))->next) : NULL)

/* List Allocators
 */
GListAllocator* g_list_allocator_new  (void);
void		g_list_allocator_free (GListAllocator* allocator);
GListAllocator* g_slist_set_allocator (GListAllocator* allocator);
GListAllocator* g_list_set_allocator  (GListAllocator* allocator);


/* Hash tables
 */
GHashTable* g_hash_table_new		(GHashFunc	 hash_func,
					 GCompareFunc	 key_compare_func);
#define g_hash_table_new_with_string_key() g_hash_table_new(g_str_hash, g_str_equal)
void	    g_hash_table_destroy	(GHashTable	*hash_table);
void	    g_hash_table_deep_destroy	(GHashTable	*hash_table);
void	    g_hash_table_insert		(GHashTable	*hash_table,
					 gpointer	 key,
					 gpointer	 value);
void	    g_hash_table_remove		(GHashTable	*hash_table,
					 gconstpointer	 key);
gpointer    g_hash_table_lookup		(GHashTable	*hash_table,
					 gconstpointer	 key);
gboolean    g_hash_table_lookup_extended(GHashTable	*hash_table,
					 gconstpointer	 lookup_key,
					 gpointer	*orig_key,
					 gpointer	*value);
void	    g_hash_table_foreach	(GHashTable	*hash_table,
					 GHFunc		 func,
					 gpointer	 user_data);
gint	    g_hash_table_size		(GHashTable	*hash_table);

/* Memory
 */

#ifdef _WIN32
typedef void (*G_MEM_CALLBACK)(gint64 index, const char* __file__, const int __line__, const char* __func__);

void g_mem_record_to(gstring path, G_MEM_CALLBACK callback);
void g_mem_record_begin();
void g_mem_record_end();

gpointer g_mem_record_malloc(gulong size, const char* __file__, const int __line__, const char* __func__);
gpointer g_mem_record_malloc0(gulong size, const char* __file__, const int __line__, const char* __func__);
gpointer g_mem_record_realloc(gpointer mem,	gulong size, const char* __file__, const int __line__, const char* __func__);
void g_mem_record_free(gpointer mem, const char* __file__, const int __line__, const char* __func__);

gpointer _g_malloc      (gulong	  size);
gpointer _g_malloc0     (gulong	  size);
gpointer _g_realloc     (gpointer  mem,	gulong	  size);
void	 _g_free	    (gpointer  mem);

#define g_malloc(size)     g_mem_record_malloc(size, __FILE__, __LINE__, __FUNCTION__)
#define g_malloc0(size)     g_mem_record_malloc0(size, __FILE__, __LINE__, __FUNCTION__)
#define g_realloc(mem, size)  g_mem_record_realloc(mem,	size, __FILE__, __LINE__, __FUNCTION__)
#define g_free(mem) g_mem_record_free(mem, __FILE__, __LINE__, __FUNCTION__)
#else
#define g_mem_record_to()
#define g_mem_record_begin()
#define g_mem_record_end()

gpointer _g_malloc      (gulong	  size);
gpointer _g_malloc0     (gulong	  size);
gpointer _g_realloc     (gpointer  mem,	gulong	  size);
void	 _g_free	    (gpointer  mem);

#define g_malloc(size)    _g_malloc(size)
#define g_malloc0(size)   _g_malloc0(size)
#define g_realloc(mem, size) _g_realloc(mem, size)
#define g_free(mem) _g_free(mem)

#endif

void	 g_mem_profile (void);
void	 g_mem_check   (gpointer  mem);


/* String utility functions
 */
#define G_STR_DELIMITERS     "_-|> <."
void	g_strdelimit		(gchar	     *string,
				 const gchar *delimiters,
				 gchar	      new_delimiter);
gchar*	g_strdup		(const gchar *str);
gchar*	g_strconcat		(const gchar *string1,
				 ...); /* NULL terminated */
gint	g_strcasecmp		(const gchar *s1,
				 const gchar *s2);
void	g_strdown		(gchar	     *string);
void	g_strup			(gchar	     *string);
void	g_strreverse		(gchar	     *string);
void g_strcpy(gchar*dst, gchar*src);
gboolean g_strstartwith(gchar *string, gchar *sub);
gboolean g_strendwith(gchar *string, gchar *sub);
gint g_strindexof(gstring fstring, gchar* str, gint index);
gint g_strlastindexof(gstring fstring, gchar* str);
gstring g_strsubstring (gstring fstring, gint st, gint len);
gint g_strgethex(gchar c);
gint g_strparseinteger (gstring fstring, gchar chend, gint base);
#define g_strtointeger(str) g_strparseinteger(str, '\0', 10)
#define g_strequal(s1, s2) (g_strcasecmp(s1, s2) == 0)
gchar* g_strreplace(gchar *source, gchar* sub, gchar* rep);
gchar* g_strreplaceandfree(gchar *source, gchar* sub, gchar* rep);
gchar* g_strtrim		(gchar *str);

#ifdef HAVE_MEMMOVE
#define g_memmove memmove
#else 
char *_memmove(char *dst, register char *src, register int n);
#define g_memmove _memmove
#endif

/* Strings
 */

gstring g_limit(gstring str, gint len);
gstring g_format(const gstring format, ...);
gwstring g_unicode(gconststring str);
gwstring g_unicode_dup(gconststring str);

#define g_utf8_length_of_wchar(c) ((c) < 0x80 ? 1 : (c) < 0x800 ? 2 : ((c) < 0x10000 ? 3 : (c) < 0x200000 ? 4 : (c) < 0x4000000 ? 5 : 6))
#define g_utf8_skip(c) ((c) < 0xC0 ? 1 : (c) < 0xE0 ? 2 : ((c) < 0xF0 ? 3 : ((c) < 0xF8 ? 4 : ((c) < 0xFC ? 5 : 6))))
#define g_utf8_next_char(p) (char *)((p) + g_utf8_skip(*(guchar *)(p)))
gstring g_utf8(gconstwstring str);
gstring g_utf8_dup(gconstwstring str);
gint g_utf8_strlen (const gchar *p, gint max);
gwchar g_utf8_get_char (const gchar *p);
int g_unichar_to_utf8 (gwchar c, gchar *outbuf);

gboolean g_is_space(gwchar c);
#define g_is_cjk(c) ((c) >= 8192)

GString* g_string_new	    (const gchar *init);
GString* g_string_wrap		(gchar *init);
GString* g_string_sized_new (guint	  dfl_size);
void	 g_string_free	    (GString	 *string);
GString* g_string_assign    (GString	 *lval,
			     const gchar *rval);
GString* g_string_truncate  (GString	 *string,
			     gint	  len);
GString* g_string_append    (GString	 *string,
			     const gchar *val);
GString* g_string_append_with_length    (GString	 *string,
							 const gchar *val, gint len);
GString* g_string_append_c  (GString	 *string,
			     gchar	  c);
GString* g_string_prepend   (GString	 *string,
			     const gchar *val);
GString* g_string_prepend_c (GString	 *string,
			     gchar	  c);
GString* g_string_insert    (GString	 *string, 
			     gint	  pos, 
			     const gchar *val);
GString* g_string_insert_c  (GString	 *string, 
			     gint	  pos, 
			     gchar	  c);
GString* g_string_erase	    (GString	 *string, 
			     gint	  pos, 
			     gint	  len);
GString* g_string_down	    (GString	 *string);
GString* g_string_up	    (GString	 *string);
void	 g_string_sprintf   (GString	 *string,
			     const gchar *format,
			     ...);
void	 g_string_sprintfa  (GString	 *string,
			     const gchar *format,
			     ...);
GString* g_string_trim (GString *fstring);
gint	 g_string_index_of(GString *fstring, gchar* str, int index);
GString*  g_string_substring (GString *fstring, gint st, gint len);
GPtrArray * g_string_split (GString *fstring, gchar * str);
gint g_string_parse_integer (GString *fstring, gchar chend, gint base);
#define g_string_to_integer(str) g_string_parse_integer(str, '\0', 10)

/* Resizable arrays
 */
#define g_array_length(array,type) \
     (((array)->len)/(gint)sizeof(type))
#define g_array_append_val(array,type,val) \
     g_rarray_append (array, (gpointer) &val, sizeof (type))
#define g_array_append_vals(array,type,vals,nvals) \
     g_rarray_append (array, (gpointer) vals, sizeof (type) * nvals)
#define g_array_prepend_val(array,type,val) \
     g_rarray_prepend (array, (gpointer) &val, sizeof (type))
#define g_array_prepend_vals(array,type,vals,nvals) \
     g_rarray_prepend (array, (gpointer) vals, sizeof (type) * nvals)
#define g_array_set_length(array,type,length) \
     g_rarray_set_length (array, length, sizeof (type))
#define g_array_index(array,type,index) \
     ((type*) array->data)[index]
#define g_array_remove_index(array,type,index) \
	 g_rarray_remove_index(array, sizeof(type), index)
#define g_array_insert_val(array, index, type, val) \
	 g_rarray_insert_val(array, index, (gpointer) &val, sizeof(type))

GArray* g_array_new	  (void);
void	g_array_free	  (GArray   *array);
GArray* g_rarray_append	  (GArray   *array,
			   gpointer  data,
			   gint	     size);
GArray* g_rarray_prepend  (GArray   *array,
			   gpointer  data,
			   gint	     size);
GArray* g_rarray_set_length (GArray   *array,
			   gint	     length,
			   gint	     size);
GArray* g_rarray_remove_index (GArray   *array,
			   gint	     size,
			   gint		 index);
GArray* g_rarray_insert_val (GArray   *array,
			   gint index,
			   gpointer  data,
			   gint      size);

/* Resizable pointer array.  This interface is much less complicated
 * than the above.  Add appends appends a pointer.  Remove fills any
 * cleared spot and shortens the array.
 */

#define	    g_ptr_array_index(array,index) (array->pdata)[index]
#define		g_ptr_array_length(array) (array->len)
GPtrArray*  g_ptr_array_new		   (void);
GPtrArray*  g_ptr_array_new_sorted (GCompareFunc	 func);
void	    g_ptr_array_free		   (GPtrArray	*array);
void	    g_ptr_array_set_size	   (GPtrArray	*array,
					    gint	 length);
void		g_ptr_array_set_capacity  (GPtrArray   *farray,
						   gint	     length);
void	    g_ptr_array_remove_index	   (GPtrArray	*array,
					    gint	 index);
gboolean    g_ptr_array_remove		   (GPtrArray	*array,
					    gpointer	 data);
gint	    g_ptr_array_add		   (GPtrArray	*array,
					    gpointer	 data);
void	    g_ptr_array_insert	   (GPtrArray	*farray,
									gint index,
									gpointer	 data);
void		g_ptr_array_add_all(GPtrArray	*array, GPtrArray	*artoadd);
gint		g_ptr_array_index_of(GPtrArray	*array, void* item);
gint		g_ptr_array_lookup(GPtrArray	*array, void* item);
gint		g_ptr_array_lookup_ex(GPtrArray	*array, void* item, GCompareFunc func);
/* Byte arrays, an array of guint8.  Implemented as a GArray,
 * but type-safe.
 */

GByteArray* g_byte_array_new	  (void);
void	    g_byte_array_free	  (GByteArray *array);

GByteArray* g_byte_array_append	  (GByteArray	*array,
				   const guint8 *data,
				   guint	 len);

GByteArray* g_byte_array_prepend  (GByteArray	*array,
				   const guint8 *data,
				   guint	 len);

GByteArray* g_byte_array_set_length (GByteArray *array,
				   gint	       length);


GTimer * g_timer_new(gint interval);
GTimer * g_timer_new_ex(gint interval, gboolean background);
void g_timer_destroy(GTimer * timer);

gboolean g_timer_is_enabled(GTimer * timer);
void g_timer_start(GTimer * timer);
void g_timer_stop(GTimer * timer);
void g_timer_add_listener(GTimer * timer, GTimerCallback callback, gpointer data);
void g_timer_remove_listener(GTimer * timer, GTimerCallback callback, gpointer data);

/* Hash Functions
 */
gint  g_str_equal (gconstpointer   v,
		   gconstpointer   v2);
guint g_str_hash  (gconstpointer v);

gint  g_str_iequal (gconstpointer   v,
				   gconstpointer   v2);
guint g_str_ihash  (gconstpointer v);

gint  g_int_equal (gconstpointer v,
		   gconstpointer v2);
guint g_int_hash  (gconstpointer v);

guint g_str_aphash(gconstpointer v);

guint g_str_aphash_with_length(gconstpointer v, guint len);

/* This "hash" function will just return the key's adress as an
 * unsigned integer. Useful for hashing on plain adresses or
 * simple integer values.
 */
guint g_direct_hash  (gconstpointer v);
gint  g_direct_equal (gconstpointer v,
		      gconstpointer v2);

#ifdef UNDER_IOT
typedef struct _GDateTime {
	gint year;		/*year*/
	gint mon;		/* month, begin from 1	*/
	gint day;		/* day,begin from  1 */
	gint hour;		/* house, 24-hour	*/
	gint min;		/* minute	*/
	gint sec;		/* second	*/
} GDateTime;
#endif
gulong g_mktime(GDateTime* time);


typedef struct  
{
	gint x;
	gint y;
} gpoint;

typedef struct  
{
	gint width;
	gint height;
} gsize;

typedef struct  
{
	gint x;
	gint y;
	gint width;
	gint height;
} grect;

typedef struct  
{
	gint left;
	gint right;
	gint top;
	gint bottom;
} gbounds;

#define g_inside(cx, cy, x, y, w, h) ((cx) >= (x) && (cy) >= (y) && (cx) < (x) + (w) && (cy) < (y) + (h))
gboolean g_rect_inside(grect * r, gint x, gint y);
gboolean g_rect_inside_of_rect(grect * r, gint x, gint y, gint w, gint h);
grect g_rect_intersect(grect a, grect b);
grect g_rect_combine(grect a, grect b);
void g_rect_set(grect * r, gint x, gint y, gint w, gint h);
gboolean g_rect_is_empty(grect * r);

void g_bounds_set(gbounds * bs, gint l, gint r, gint t, gint b);

/* Glib static variable clean
*/

//void g_mem_static_clean_gmcore(void);

gstring g_base64_encode(const gstring data, gint input_length, gint *output_length);
gstring g_base64_decode(const gstring data, gint input_length, gint *output_length);

gint g_random(gint max);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#ifdef UNDER_IOT
typedef gint VMINT;
typedef guint VMUINT;

typedef gint8 VMCHAR;
typedef gint16 VMWCHAR;

typedef gwstring VMWSTR;

typedef gint VMFILE;

typedef guchar VMBYTE;

#define VM_TCP_EVT_CONNECTED	1
#define VM_TCP_EVT_CAN_WRITE	2
#define VM_TCP_EVT_CAN_READ		3
#define VM_TCP_EVT_PIPE_BROKEN	4
#define VM_TCP_EVT_HOST_NOT_FOUND	5
#define VM_TCP_EVT_PIPE_CLOSED	6

#define VM_TCP_ERR_NO_ENOUGH_RES	-1
#define VM_TCP_ERR_CREATE_FAILED	-2

#define VM_TCP_READ_EOF				-1

#define MODE_READ					        0x01
#define MODE_WRITE					      0x02
#define MODE_CREATE_ALWAYS_WRITE	0x04
#define MODE_APPEND					      0x08

#define BASE_BEGIN		1
#define BASE_CURR		2
#define BASE_END		3

#ifndef MAX_APP_NAME_LEN
#define MAX_APP_NAME_LEN					(260)
#endif

/*
 * file attribution
 */
#define VM_FS_ATTR_READ_ONLY        	0x01
#define VM_FS_ATTR_HIDDEN           	0x02
#define VM_FS_ATTR_SYSTEM           	0x04
#define VM_FS_ATTR_VOLUME          	0x08
#define VM_FS_ATTR_DIR              		0x10
#define VM_FS_ATTR_ARCHIVE          	0x20
#define VM_FS_LONGNAME_ATTR         	0x0F

typedef GDateTime vm_time_t;
typedef  struct  vm_fileinfo_ext
{
  /* file name without path */
  VMWCHAR filefullname[MAX_APP_NAME_LEN];
  /* file name character array by 8.3 format encoding with UCS2, and the last character may be not '\0' */
  VMCHAR filename[8];
  /* file extention name character array by 8.3 format encoding with UCS2, and the last character may be not '\0' */
  VMCHAR extension[3];/* file attributes */
  VMBYTE           attributes; 
  vm_time_t       create_datetime; /* create time */
  vm_time_t	    modify_datetime; /* modify time */
  VMUINT           filesize; /* file size */
  VMUINT           drive; /* drive */
  VMUINT           stamp; /* stamp */
} vm_fileinfo_ext;

#define g_create_timer_ex g_create_timer

#define vm_file_read g_file_read
#define vm_file_write g_file_write
#define vm_file_commit g_file_commit
#define vm_file_seek g_file_seek
#define vm_file_tell g_file_tell
#define vm_file_is_eof g_file_is_eof

#define vm_file_delete g_real_file_delete
#define vm_file_rename g_real_file_rename
#define vm_file_open g_real_file_open
#define vm_file_getfilesize g_real_file_getfilesize
#define vm_file_close g_real_file_close
#define vm_file_mkdir g_real_file_mkdir

extern VMINT g_file_read(VMFILE handle, void * data, VMUINT length, VMUINT *nread);
extern VMINT g_file_write(VMFILE handle, void * data, VMUINT length, VMUINT * written);
extern VMINT g_file_commit(VMFILE handle);
extern VMINT g_file_seek(VMFILE handle, VMINT offset, VMINT base);
extern VMINT g_file_tell(VMFILE handle);
extern VMINT g_file_is_eof(VMFILE handle);
extern gboolean g_dir_exists(gstring path);
extern gboolean g_file_exists(gstring path);

extern VMINT g_real_file_delete(gstring path);
extern VMINT g_real_file_rename(gstring oldpath, gstring newpath);
extern VMINT g_real_file_open(gstring path, VMINT mode, VMINT binary);
extern VMINT g_real_file_getfilesize(VMFILE file_handle, VMUINT * size);
extern void g_real_file_close(VMFILE file_handle);
extern VMINT g_real_file_mkdir(gstring path);

extern gint g_tcp_write(gint s, gpointer buffer, gint buflen);
extern gint g_tcp_read(gint s, gpointer buffer, gint buflen);
extern gint g_tcp_close(gint s);
extern gint g_tcp_connect(char* hostname, gint port, gint apn, ghandle callback);
extern glong g_get_tick_count(void);
extern ghandle g_create_timer(guint32 millisec, ghandle vTimerCallback);

#endif

#endif /* __G_LIB_H__ */
