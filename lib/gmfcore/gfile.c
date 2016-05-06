#include "include/gfile.h"
#ifdef UNDER_MRE
#include "vmio.h"
#endif
gstring g_file_get_normalize_path(gstring path)
{
	gint pos;
	pos = g_strindexof(path, "://", 0);
	if (pos > 0)
		path = path + pos + 3;
	path = g_strreplace(path, "/", "\\");
	if (strlen(path) > 1 && path[1] == ':' && path[0] >= 'A' && path[0] <= 'Z')
	{
		path[0] = path[0] + 'a' - 'A';
	}
	return path;
}

void g_file_delete(gstring path)
{
	g_return_if_fail(path != NULL);
	if (g_file_exists(path)) {
		VMINT ret;
		path = g_file_get_normalize_path(path);
#ifdef UNDER_MRE    
		ret = vm_file_delete((VMWSTR)g_unicode(path));
#endif
#ifdef UNDER_IOT
		ret = vm_file_delete(path);
#endif    
		g_log_info("delete file[%s],ret=[%d]", path, ret);
		g_free(path);
	}
}
void g_file_rename(gstring oldpath, gstring newpath)
{
	gwstring woldpath, wnewpath;
	gstring noldpath, nnewpath;
	g_return_if_fail(oldpath != NULL && newpath != NULL);
	g_file_delete(newpath);
	noldpath = g_file_get_normalize_path(oldpath);
	woldpath = g_unicode_dup(noldpath);
	nnewpath = g_file_get_normalize_path(newpath);
	wnewpath = g_unicode_dup(nnewpath);
#ifdef UNDER_MRE  
	vm_file_rename((VMWSTR)woldpath, (VMWSTR)wnewpath);
#endif
#ifdef UNDER_IOT
  vm_file_rename(oldpath, newpath);
#endif  
	g_free(noldpath);
	g_free(woldpath);
	g_free(nnewpath);
	g_free(wnewpath);
}
#ifdef UNDER_MRE
gboolean g_dir_exists(gstring path)
{
	VMINT find_result;
	vm_fileinfo_ext info_ext;
	path = g_file_get_normalize_path(path);	
	find_result = vm_find_first_ext((VMWSTR)g_unicode(path), &info_ext);
	g_free(path);
	vm_find_close(find_result);
	return find_result >= 0 && VM_FS_ATTR_DIR == (info_ext.attributes & VM_FS_ATTR_DIR);
}
gboolean g_file_exists(gstring path)
{
	VMINT find_result;
	vm_fileinfo_ext info_ext;
	path = g_file_get_normalize_path(path);	
	find_result = vm_find_first_ext((VMWSTR)g_unicode(path), &info_ext);
	g_free(path);
	vm_find_close(find_result);
	return find_result >= 0;
}
#endif
gint g_file_get_size(gstring path)
{
	VMFILE file_handle;
	VMUINT size;
	path = g_file_get_normalize_path(path);
#ifdef UNDER_MRE
	file_handle = vm_file_open((VMWSTR)g_unicode(path), MODE_READ, TRUE);
#endif
#ifdef UNDER_IOT
  file_handle = vm_file_open(path, MODE_READ, TRUE);
#endif  
	g_free(path);
	if (file_handle < 0) return 0;
	vm_file_getfilesize(file_handle, &size);
	vm_file_close(file_handle);
	return (gint)size;
}
void g_file_make_full_directory(gstring path)
{
	gint r;
	gint pos = 0;
	gstring tmp = g_file_get_normalize_path(path);
	pos = g_strindexof(tmp, "\\", pos);
	while((pos = g_strindexof(tmp, "\\", pos + 1)) > 0)
	{
		tmp[pos] = '\0';
		if (!g_dir_exists(tmp))
		{
#ifdef UNDER_MRE      
			r = vm_file_mkdir((VMWSTR)g_unicode(tmp));
#endif
#ifdef UNDER_IOT
      r = vm_file_mkdir(tmp);
#endif      
			g_log_debug("vm_file_mkdir : %d, %s", r, tmp);
		}
		tmp[pos] = '\\';
	}
#ifdef UNDER_MRE      
	r = vm_file_mkdir((VMWSTR)g_unicode(tmp));
#endif
#ifdef UNDER_IOT
  r = vm_file_mkdir(tmp);
#endif
	g_free(tmp);
}

void g_file_make_full_directory_for_file(gstring path)
{
	gint r;
	gint pos = 0;
	gstring tmp = g_file_get_normalize_path(path);
	pos = g_strindexof(tmp, "\\", pos);
	while((pos = g_strindexof(tmp, "\\", pos + 1)) > 0)
	{
		tmp[pos] = '\0';
		if (!g_dir_exists(tmp))
		{
#ifdef UNDER_MRE      
			r = vm_file_mkdir((VMWSTR)g_unicode(tmp));
#endif
#ifdef UNDER_IOT
      r = vm_file_mkdir(tmp);
#endif      
			g_log_debug("vm_file_mkdir : %d, %s", r, tmp);
		}
		tmp[pos] = '\\';
	}
	g_free(tmp);
}