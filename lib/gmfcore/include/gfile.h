#ifndef _G_FILE_H_
#define _G_FILE_H_
#include "glib.h"

gstring g_file_get_normalize_path(gstring path);
gboolean g_file_exists(gstring path);
gboolean g_dir_exists(gstring path);
gint g_file_get_size(gstring path);
void g_file_make_full_directory(gstring path);
void g_file_make_full_directory_for_file(gstring path);
void g_file_delete(gstring path);
void g_file_rename(gstring oldpath, gstring newpath);
#endif