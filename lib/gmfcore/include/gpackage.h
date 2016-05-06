#ifndef _G_PACKAGE_H_
#define _G_PACKAGE_H_
#include "gstream.h"
typedef struct
{
	GStream * stream;
	GHashTable * items;
} GPackage;

typedef struct
{
	GPackage * package;
	gint start;
	gint length;
} GPackageItem;

GPackage * g_package_new(void);
void g_package_free(GPackage* self);
gboolean g_package_open(GPackage * self, gstring fn);
GPackageItem * g_package_get_item(GPackage * self, gstring fn);
void g_package_register(gstring packagename, gstring path);
void g_package_unregister(gstring packagename);
gboolean g_package_is_valid(gstring fn);
GPackage* g_package_get(gstring packagename);
gstring g_package_read_as_string(gstring packagename, gstring path);
void g_package_read(gstring packagename, gstring path, gpointer buffer);
void g_package_item_read(GPackageItem * item, gpointer buffer);

#endif
