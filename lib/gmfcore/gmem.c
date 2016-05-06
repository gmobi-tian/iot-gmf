#include "include/glib.h"

/* #define ENABLE_MEM_PROFILE */
/* #define ENABLE_MEM_CHECK */


#define MAX_MEM_AREA  65536L
#define MEM_AREA_SIZE 4L

#if SIZEOF_VOID_P > SIZEOF_LONG
#define MEM_ALIGN     SIZEOF_VOID_P
#else
#define MEM_ALIGN     SIZEOF_LONG
#endif

#ifndef HAVE_MEMMOVE
char *_memmove(char *dst, register char *src, register int n)
{
    register char *svdst;

    if ((dst > src) && (dst < src + n)) 
    {
        src += n;
        for (svdst = dst + n; n-- > 0; )
            *--svdst = *--src;
    }
    else
    {
        for (svdst = dst; n-- > 0; )
            *svdst++ = *src++;
    }
    return dst;
}
#endif

typedef struct _GMemArea       GMemArea;
typedef struct _GRealMemChunk  GRealMemChunk;

struct _GMemArea
{
  GMemArea *next;            /* the next mem area */
  GMemArea *prev;            /* the previous mem area */
  gulong index;              /* the current index into the "mem" array */
  gulong free;               /* the number of free bytes in this mem area */
  gulong allocated;          /* the number of atoms allocated from this area */
  gulong mark;               /* is this mem area marked for deletion */
  gchar mem[MEM_AREA_SIZE];  /* the mem array from which atoms get allocated
			      * the actual size of this array is determined by
			      *  the mem chunk "area_size". ANSI says that it
			      *  must be declared to be the maximum size it
			      *  can possibly be (even though the actual size
			      *  may be less).
			      */
};

struct _GRealMemChunk
{
  gchar *name;               /* name of this MemChunk...used for debugging output */
  gint type;                 /* the type of MemChunk: ALLOC_ONLY or ALLOC_AND_FREE */
  gint num_mem_areas;        /* the number of memory areas */
  gint num_marked_areas;     /* the number of areas marked for deletion */
  guint atom_size;           /* the size of an atom */
  gulong area_size;          /* the size of a memory area */
  GMemArea *mem_area;        /* the current memory area */
  GMemArea *mem_areas;       /* a list of all the mem areas owned by this chunk */
  GMemArea *free_mem_area;   /* the free area...which is about to be destroyed */
  GFreeAtom *free_atoms;     /* the free atoms list */
  GTree *mem_tree;           /* tree of mem areas sorted by memory address */
  GRealMemChunk *next;       /* pointer to the next chunk */
  GRealMemChunk *prev;       /* pointer to the previous chunk */
};


static gulong g_mem_chunk_compute_size (gulong    size);
static gint   g_mem_chunk_area_compare (GMemArea *a,
					GMemArea *b);
static gint   g_mem_chunk_area_search  (GMemArea *a,
					gchar    *addr);


static GRealMemChunk *mem_chunks = NULL;

#ifdef ENABLE_MEM_PROFILE
static gulong allocations[4096] = { 0 };
static gulong allocated_mem = 0;
static gulong freed_mem = 0;
#endif /* ENABLE_MEM_PROFILE */


gpointer
_g_malloc (gulong size)
{
  gpointer p;


#if defined(ENABLE_MEM_PROFILE) || defined(ENABLE_MEM_CHECK)
  gulong *t;
#endif /* ENABLE_MEM_PROFILE || ENABLE_MEM_CHECK */


  if (size == 0)
    return NULL;


#if defined(ENABLE_MEM_PROFILE) || defined(ENABLE_MEM_CHECK)
  size += SIZEOF_LONG;
#endif /* ENABLE_MEM_PROFILE || ENABLE_MEM_CHECK */

#ifdef ENABLE_MEM_CHECK
  size += SIZEOF_LONG;
#endif /* ENABLE_MEM_CHECK */

  //p = (gpointer) malloc (size);
  p = (gpointer) calloc (size, 1);
  if (!p)
    g_error ("could not allocate %ld bytes", size);


#ifdef ENABLE_MEM_CHECK
  size -= SIZEOF_LONG;

  t = p;
  p = ((guchar*) p + SIZEOF_LONG);
  *t = 0;
#endif /* ENABLE_MEM_CHECK */

#if defined(ENABLE_MEM_PROFILE) || defined(ENABLE_MEM_CHECK)
  size -= SIZEOF_LONG;

  t = p;
  p = ((guchar*) p + SIZEOF_LONG);
  *t = size;

#ifdef ENABLE_MEM_PROFILE
  if (size <= 4095)
    allocations[size-1] += 1;
  else
    allocations[4095] += 1;
  allocated_mem += size;
#endif /* ENABLE_MEM_PROFILE */
#endif /* ENABLE_MEM_PROFILE || ENABLE_MEM_CHECK */


  return p;
}

gpointer
_g_malloc0 (gulong size)
{
	gpointer p;


#if defined(ENABLE_MEM_PROFILE) || defined(ENABLE_MEM_CHECK)
	gulong *t;
#endif /* ENABLE_MEM_PROFILE || ENABLE_MEM_CHECK */


	if (size == 0)
		return NULL;


#if defined(ENABLE_MEM_PROFILE) || defined(ENABLE_MEM_CHECK)
	size += SIZEOF_LONG;
#endif /* ENABLE_MEM_PROFILE || ENABLE_MEM_CHECK */

#ifdef ENABLE_MEM_CHECK
	size += SIZEOF_LONG;
#endif /* ENABLE_MEM_CHECK */


	p = (gpointer) calloc (size, 1);
	if (!p)
		g_error ("could not allocate %ld bytes", size);


#ifdef ENABLE_MEM_CHECK
	size -= SIZEOF_LONG;

	t = p;
	p = ((guchar*) p + SIZEOF_LONG);
	*t = 0;
#endif /* ENABLE_MEM_CHECK */

#if defined(ENABLE_MEM_PROFILE) || defined(ENABLE_MEM_CHECK)
	size -= SIZEOF_LONG;

	t = p;
	p = ((guchar*) p + SIZEOF_LONG);
	*t = size;

#ifdef ENABLE_MEM_PROFILE
	if (size <= 4095)
		allocations[size-1] += 1;
	else
		allocations[4095] += 1;
	allocated_mem += size;
#endif /* ENABLE_MEM_PROFILE */
#endif /* ENABLE_MEM_PROFILE || ENABLE_MEM_CHECK */


	return p;
}

gpointer
_g_realloc (gpointer mem,
	   gulong   size)
{
  gpointer p;

#if defined(ENABLE_MEM_PROFILE) || defined(ENABLE_MEM_CHECK)
  gulong *t;
#endif /* ENABLE_MEM_PROFILE || ENABLE_MEM_CHECK */


  if (size == 0)
    return NULL;


#if defined(ENABLE_MEM_PROFILE) || defined(ENABLE_MEM_CHECK)
  size += SIZEOF_LONG;
#endif /* ENABLE_MEM_PROFILE || ENABLE_MEM_CHECK */

#ifdef ENABLE_MEM_CHECK
  size += SIZEOF_LONG;
#endif /* ENABLE_MEM_CHECK */


  if (!mem)
    p = (gpointer) malloc (size);
  else
    {
#if defined(ENABLE_MEM_PROFILE) || defined(ENABLE_MEM_CHECK)
      t = (gulong*) ((guchar*) mem - SIZEOF_LONG);
#ifdef ENABLE_MEM_PROFILE
      freed_mem += *t;
#endif /* ENABLE_MEM_PROFILE */
      mem = t;
#endif /* ENABLE_MEM_PROFILE || ENABLE_MEM_CHECK */

#ifdef ENABLE_MEM_CHECK
      t = (gulong*) ((guchar*) mem - SIZEOF_LONG);
      if (*t >= 1)
	g_warning ("trying to realloc freed memory\n");
      mem = t;
#endif /* ENABLE_MEM_CHECK */

      p = (gpointer) realloc (mem, size);
    }

  if (!p)
    g_error ("could not reallocate %ld bytes", size);


#ifdef ENABLE_MEM_CHECK
  size -= SIZEOF_LONG;

  t = p;
  p = ((guchar*) p + SIZEOF_LONG);
  *t = 0;
#endif /* ENABLE_MEM_CHECK */

#if defined(ENABLE_MEM_PROFILE) || defined(ENABLE_MEM_CHECK)
  size -= SIZEOF_LONG;

  t = p;
  p = ((guchar*) p + SIZEOF_LONG);
  *t = size;

#ifdef ENABLE_MEM_PROFILE
  if (size <= 4095)
    allocations[size-1] += 1;
  else
    allocations[4095] += 1;
  allocated_mem += size;
#endif /* ENABLE_MEM_PROFILE */
#endif /* ENABLE_MEM_PROFILE || ENABLE_MEM_CHECK */


  return p;
}

void
_g_free (gpointer mem)
{
  if (mem)
    {
#if defined(ENABLE_MEM_PROFILE) || defined(ENABLE_MEM_CHECK)
      gulong *t;
      gulong size;
#endif /* ENABLE_MEM_PROFILE || ENABLE_MEM_CHECK */

#if defined(ENABLE_MEM_PROFILE) || defined(ENABLE_MEM_CHECK)
      t = (gulong*) ((guchar*) mem - SIZEOF_LONG);
      size = *t;
#ifdef ENABLE_MEM_PROFILE
      freed_mem += size;
#endif /* ENABLE_MEM_PROFILE */
      mem = t;
#endif /* ENABLE_MEM_PROFILE || ENABLE_MEM_CHECK */

#ifdef ENABLE_MEM_CHECK
      t = (gulong*) ((guchar*) mem - SIZEOF_LONG);
      if (*t >= 1)
	g_warning ("freeing previously freed memory\n");
      *t += 1;
      mem = t;

      memset ((guchar*) mem + 8, 0, size);
#else /* ENABLE_MEM_CHECK */
      free (mem);
#endif /* ENABLE_MEM_CHECK */
    }
}
#ifdef _WIN32
#include "vmio.h"
#include "vmchset.h"
static VMINT mem_record_file = 0;
static VMINT mem_record_enabled = 0;
static char mem_record_log[1024];
static gint64 mem_record_index = 0;
static G_MEM_CALLBACK mem_record_callback = NULL;
void g_mem_record_to(gstring path, G_MEM_CALLBACK callback)
{
	VMWCHAR wpath[128];
	vm_ascii_to_ucs2(wpath, 128, (VMSTR)path);
	mem_record_file = vm_file_open(wpath, MODE_CREATE_ALWAYS_WRITE, 1);
	mem_record_callback = callback;
}
void g_mem_record_begin()
{
	mem_record_enabled = 1;
}
void g_mem_record_end()
{
	mem_record_enabled = 0;
}
gpointer g_mem_record_malloc(gulong size, const char* __file__, const int __line__, const char* __func__)
{
	gpointer mem = _g_malloc(size);
	if (mem_record_enabled)
	{
		VMUINT written;
		if (mem_record_callback)
			mem_record_callback(mem_record_index, __file__, __line__, __func__);
		sprintf(mem_record_log, "+\t%u\t%u\t%s\t%d\t%s\t%ld\r\n", mem, size, __file__, __line__, __func__, mem_record_index++);
		vm_file_write(mem_record_file, mem_record_log, strlen(mem_record_log), &written);
		vm_file_commit(mem_record_file);
	}
	return mem;
}
gpointer g_mem_record_malloc0(gulong size, const char* __file__, const int __line__, const char* __func__)
{
	gpointer mem = _g_malloc0(size);
	if (mem_record_enabled)
	{
		VMUINT written;
		if (mem_record_callback) mem_record_callback(mem_record_index, __file__, __line__, __func__);
		sprintf(mem_record_log, "+\t%u\t%u\t%s\t%d\t%s\t%ld\r\n", mem, size, __file__, __line__, __func__, mem_record_index++);
		vm_file_write(mem_record_file, mem_record_log, strlen(mem_record_log), &written);
		vm_file_commit(mem_record_file);
	}
	return mem;
}
gpointer g_mem_record_realloc(gpointer mem,	gulong size, const char* __file__, const int __line__, const char* __func__)
{
	gpointer memnew = _g_realloc(mem, size);
	if (mem_record_enabled)
	{
		VMUINT written;
		if (mem != NULL)
		{
			if (mem_record_callback) mem_record_callback(mem_record_index, __file__, __line__, __func__);
			sprintf(mem_record_log, "-\t%u\t%s\t%d\t%s\t%ld\r\n", mem, __file__, __line__, __func__, mem_record_index++);
			vm_file_write(mem_record_file, mem_record_log, strlen(mem_record_log), &written);
			vm_file_commit(mem_record_file);
		}

		if (mem_record_callback) mem_record_callback(mem_record_index, __file__, __line__, __func__);
		sprintf(mem_record_log, "+\t%u\t%u\t%s\t%d\t%s\t%ld\r\n", memnew, size, __file__, __line__, __func__, mem_record_index++);
		vm_file_write(mem_record_file, mem_record_log, strlen(mem_record_log), &written);
		vm_file_commit(mem_record_file);
	}
	return memnew;
}
void g_mem_record_free(gpointer mem, const char* __file__, const int __line__, const char* __func__)
{
	if (mem_record_enabled)
	{
		VMUINT written;
		if (mem_record_callback) mem_record_callback(mem_record_index, __file__, __line__, __func__);
		sprintf(mem_record_log, "-\t%u\t%s\t%d\t%s\t%ld\r\n", mem, __file__, __line__, __func__, mem_record_index++);
		vm_file_write(mem_record_file, mem_record_log, strlen(mem_record_log), &written);
		vm_file_commit(mem_record_file);
	}
	_g_free(mem);
}
#endif

void
g_mem_profile (void)
{
#ifdef ENABLE_MEM_PROFILE
  gint i;

  for (i = 0; i < 4095; i++)
    if (allocations[i] > 0)
      g_print ("%lu allocations of %d bytes\n", allocations[i], i + 1);

  if (allocations[4095] > 0)
    g_print ("%lu allocations of greater than 4095 bytes\n", allocations[4095]);
  g_print ("%lu bytes allocated\n", allocated_mem);
  g_print ("%lu bytes freed\n", freed_mem);
  g_print ("%lu bytes in use\n", allocated_mem - freed_mem);
#endif /* ENABLE_MEM_PROFILE */
}

void
g_mem_check (gpointer mem)
{
#ifdef ENABLE_MEM_CHECK
  gulong *t;

  t = (gulong*) ((guchar*) mem - SIZEOF_LONG - SIZEOF_LONG);

  if (*t >= 1)
    g_warning ("mem: 0x%08x has been freed: %lu\n", (gulong) mem, *t);
#endif /* ENABLE_MEM_CHECK */
}

