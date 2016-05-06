#ifndef __G_OBJECT_H__
#define __G_OBJECT_H__

#include "glib.h"
#include "gvalue.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

typedef gint (*GObjectCallMethod) (GObject * self, const gchar * name, ...);
typedef void (*GObjectSetProperty) (GObject * self, const gchar * key, GValue val);
typedef GValue (*GObjectGetProperty) (GObject * self, const gchar * key);

typedef void (*GObjectClassInitFunc) (gpointer clazz);
typedef void (*GObjectClassFinalizeFunc) (gpointer clazz);

typedef void (*GObjectInitFunc) (gpointer object);
typedef void (*GObjectFinalizeFunc) (gpointer object);

struct _GObject
{
	/*< private >*/
	GObjectClass * g_class;

	//GHashTable* properties;
};

// all methods in class are virtual methods, otherwise you should not add them in the class
struct _GObjectClass
{
	/*< private >*/
	gchar *                  g_class_name;
	GObjectClassFinalizeFunc g_class_finalize;
	GObjectClass *			 g_base_class;

	gint                     instance_size;

	GObjectInitFunc          init;
	GObjectFinalizeFunc      finalize;
};

GObject * g_new_object(GObjectClass * clazz);
void g_delete_object(gpointer self);

GObjectClass * g_register_class(
	const gchar * classname, GObjectClass * baseclazz, gint class_size, GObjectClassInitFunc class_init, GObjectClassFinalizeFunc class_finalize,
	gint instance_size, GObjectInitFunc init, GObjectFinalizeFunc finalize);
GObjectClass * g_get_class(const gchar * classname);
GObjectClass * g_get_class_GObject(void);
void g_object_init(gpointer self);
void g_object_finalize(gpointer self);
void g_object_class_init(gpointer clazz);
const gchar * g_get_class_name(GObjectClass * clazz);
gboolean g_is_type_of(GObjectClass * thisclazz, GObjectClass * targetclazz);
gboolean g_is_instance_of(gpointer self, GObjectClass * targetclazz);

#define GOBJECT(p) ((GObject*)(p))
#define CLASS(class) (GObjectClass *)g_get_class_##class()
#define CLASS_NAME(class) #class "Class"
#define REGISTER_CLASS(class, baseclass, init, finalize, class_init, class_finalize) g_register_class(#class "Class", CLASS(baseclass), sizeof(class ## Class), class_init, class_finalize, sizeof(class), init, finalize);


void g_object_set_property(GObject * self, const gchar * name, GValue * val);
GValue * g_object_get_property(GObject * self, const gchar * name);
/*
gint g_object_call(GObject * self, const gchar * name, ...);
void g_object_set(GObject * self, const gchar * key, GValue val);
GValue g_object_get(GObject * self, const gchar * key);

void g_object_test(GObject * self, const gchar * name, ...);
*/

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
