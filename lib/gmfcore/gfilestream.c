#include "include/glib.h"
#include "include/gfilestream.h"
#include "include/gfile.h"
#ifdef UNDER_MRE
#include "vmio.h"
#endif
static int open_file_count = 0;
#define GET_MODE(mode) (mode == FILE_MODE_READ_ONLY ? MODE_READ : (mode == FILE_MODE_READ_WRITE ? MODE_WRITE : MODE_CREATE_ALWAYS_WRITE))

void g_file_stream_reopen(gpointer self)
{
	GFileStream * s = GFILESTREAM(self);	
	g_return_if_fail(self != NULL && !g_file_stream_is_open(self));
#ifdef UNDER_MRE  
	s->file_handle = (ghandle)vm_file_open((VMWSTR)g_unicode(s->file_name), s->file_mode == FILE_MODE_READ_ONLY ? MODE_READ : MODE_WRITE, TRUE);
#endif
#ifdef UNDER_IOT
	s->file_handle = (ghandle)vm_file_open(s->file_name, s->file_mode == FILE_MODE_READ_ONLY ? MODE_READ : MODE_WRITE, TRUE);
#endif  
	if (g_file_stream_is_open(s))
		open_file_count ++;
	g_log_debug("open file %s : %d (count = %d)", s->file_name, s->file_handle, open_file_count);
}
void g_file_stream_open(gpointer self, gstring name, GFileMode mode)
{
	GFileStream * s = GFILESTREAM(self);	
	g_return_if_fail(self != NULL && !g_file_stream_is_open(self));
	if (GFILESTREAM(self)->file_name)
		g_free(GFILESTREAM(self)->file_name);
	s->file_name = g_file_get_normalize_path(name);
	s->file_mode = mode;
	//s->file_handle = (ghandle)vm_file_open(g_unicode(name), MODE_CREATE_ALWAYS_WRITE, TRUE);	
#ifdef UNDER_MRE
	s->file_handle = (ghandle)vm_file_open((VMWSTR)g_unicode(s->file_name), GET_MODE(s->file_mode), TRUE);
#endif
#ifdef UNDER_IOT
	s->file_handle = (ghandle)vm_file_open(s->file_name, GET_MODE(s->file_mode), TRUE);  
#endif  
	if (g_file_stream_is_open(s))
		open_file_count ++;
	g_log_debug("open file %s : %d (count = %d)", s->file_name, s->file_handle, open_file_count);
}
gboolean g_file_stream_is_open(gpointer self)
{
	GFileStream * s = GFILESTREAM(self);
	g_return_val_if_fail(self != NULL, FALSE);
	return (VMFILE)s->file_handle >= 0;
}
void g_file_stream_set_length(gpointer self, gint len)
{

}
gint g_file_stream_get_length(gpointer self)
{
	VMUINT size = 0;
	GFileStream * s = GFILESTREAM(self);
	g_return_val_if_fail(g_file_stream_is_open(self), 0);
	if (vm_file_getfilesize((VMFILE)s->file_handle, &size) < 0)
		g_log_error("g_file_stream_get_length : fails to get file size!");
	return (gint)size;
}
gint g_file_stream_get_position(gpointer self)
{
	GFileStream * s = GFILESTREAM(self);
	g_return_val_if_fail(g_file_stream_is_open(self), 0);
	return (gint)vm_file_tell((VMFILE)s->file_handle);
}
gint g_file_stream_write(gpointer self, void * buffer, gint length)
{
	VMUINT written = 0;
	GFileStream * s = GFILESTREAM(self);
	g_return_val_if_fail(g_file_stream_is_open(self), 0);
	vm_file_write((VMFILE)s->file_handle, buffer, (VMUINT)length, &written);
	return (gint)written;
}
gint g_file_stream_read(gpointer self, void * buffer, gint length)
{
	VMUINT nread = 0;
	GFileStream * s = GFILESTREAM(self);
	g_return_val_if_fail(g_file_stream_is_open(self), 0);
	vm_file_read((VMFILE)s->file_handle, buffer, (VMUINT)length, &nread);
	return (gint)nread;
}
void g_file_stream_seek(gpointer self, gint offset, GSeekOrigin origin)
{
	GFileStream * s = GFILESTREAM(self);
	g_return_if_fail(g_file_stream_is_open(self));
	switch(origin)
	{
	case SEEK_BEGIN:
		vm_file_seek((VMFILE)s->file_handle, offset, BASE_BEGIN);
		break;
	case SEEK_CURRENT:
		vm_file_seek((VMFILE)s->file_handle, offset, BASE_CURR);
		break;
	case SEEK_END:
		vm_file_seek((VMFILE)s->file_handle, offset, BASE_END);
		break;
	}
}
void g_file_stream_close(gpointer self)
{
	if (g_file_stream_is_open(self))
	{
		open_file_count --;
		vm_file_close((VMFILE)GFILESTREAM(self)->file_handle);
		g_log_debug("close file %s : %d (count = %d)", GFILESTREAM(self)->file_name, GFILESTREAM(self)->file_handle, open_file_count);
		GFILESTREAM(self)->file_handle = (ghandle)-1;
	}
}
void g_file_stream_flush(gpointer self)
{
	GFileStream * s = GFILESTREAM(self);
	g_return_if_fail(g_file_stream_is_open(self));
	vm_file_commit((VMFILE)s->file_handle);
}
gboolean g_file_stream_is_eof(gpointer self)
{
	GFileStream * s = GFILESTREAM(self);
	g_return_val_if_fail(g_file_stream_is_open(self), TRUE);
	return vm_file_is_eof((VMFILE)s->file_handle);
}

void g_file_stream_init(gpointer self)
{
	g_stream_init(self);
	GFILESTREAM(self)->file_handle = (ghandle)-1;
	GFILESTREAM(self)->file_name = NULL;
}
void g_file_stream_finalize(gpointer self)
{
	if (g_file_stream_is_open(self))
		g_file_stream_close(self);
	if (GFILESTREAM(self)->file_name)
		g_free(GFILESTREAM(self)->file_name);
	GFILESTREAM(self)->file_name = NULL;
	g_stream_finalize(self);
}
void g_file_stream_class_init(gpointer c)
{
	g_stream_class_init(c);
	GSTREAMCLASS(c)->get_length = g_file_stream_get_length;
	GSTREAMCLASS(c)->set_length = g_file_stream_set_length;
	GSTREAMCLASS(c)->get_position = g_file_stream_get_position;
	GSTREAMCLASS(c)->read = g_file_stream_read;
	GSTREAMCLASS(c)->write = g_file_stream_write;
	GSTREAMCLASS(c)->seek = g_file_stream_seek;
	GSTREAMCLASS(c)->flush = g_file_stream_flush;
	GSTREAMCLASS(c)->is_eof = g_file_stream_is_eof;
}

gpointer g_get_class_GFileStream()
{
	gpointer clazz = g_get_class(CLASS_NAME(GFileStream));
	return clazz != NULL ? clazz :
		REGISTER_CLASS(GFileStream, GStream, g_file_stream_init, g_file_stream_finalize, g_file_stream_class_init, NULL);
}

