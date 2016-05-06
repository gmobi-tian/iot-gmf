#ifndef _G_RESOURCE_STREAM_H_
#define _G_RESOURCE_STREAM_H_
#include "gstream.h"

#define GRESOURCESTREAM(p) ((GResourceStream *)(p))
#define GRESOURCESTREAMCLASS(p) ((GResourceStreamClass *)(p))
typedef struct _GResourceStream GResourceStream;
typedef struct _GResourceStreamClass GResourceStreamClass;

struct _GResourceStream
{
	GStream base;
	gstring name;
	gint position;
};
struct _GResourceStreamClass
{
	GStreamClass base;
};

void g_resource_stream_open(gpointer self, gstring name);
void g_resource_stream_close(gpointer self);
gint g_resource_stream_get_length(gpointer self);
void g_resource_stream_set_length(gpointer self, gint len);
gint g_resource_stream_get_position(gpointer self);
gint g_resource_stream_write(gpointer self, void * buffer, gint length);
gint g_resource_stream_read(gpointer self, void * buffer, gint length);
void g_resource_stream_seek(gpointer self, gint offset, GSeekOrigin origin);
void g_resource_stream_flush(gpointer self);
gboolean g_resource_stream_is_eof(gpointer self);
gboolean g_resource_stream_is_open(gpointer self);

gpointer g_get_class_GResourceStream(void);

void g_resource_stream_init(gpointer self);
void g_resource_stream_finalize(gpointer self);
void g_resource_stream_class_init(gpointer clazz);

#endif
