#ifndef _G_DOWNLOADER_H_
#define _G_DOWNLOADER_H_

#include "ghttp.h"

typedef void (*DOWNLOADING_FUNC)(GHttpSession * session, gstring url, gstring path, gpointer data);
typedef void (*DOWNLOADED_FUNC)(GHttpSession * session, gstring url, gstring path, gpointer data);

typedef struct
{
	GList * items;
	GHttpSession * session;
} GDownloader;

typedef struct  
{
	GDownloader * downloader;
	gstring url;
	gstring path;
	DOWNLOADING_FUNC downloading;
	DOWNLOADED_FUNC downloaded;
	gpointer data;
	gboolean resuming;
}GDownloaderItem;


GDownloader * g_downloader_new(void);
void g_downloader_free(GDownloader * self);
GDownloaderItem * g_downloader_add_task(GDownloader * self, gstring url, gstring path, gpointer data, DOWNLOADED_FUNC downloaded, DOWNLOADING_FUNC downloading);
GDownloaderItem * g_downloader_add_resuming_task(GDownloader * self, gstring url, gstring path, gpointer data, DOWNLOADED_FUNC downloaded, DOWNLOADING_FUNC downloading);
GDownloaderItem * g_downloader_get_task(GDownloader * self, gstring url);
void g_downloader_remove_task(GDownloader * self, gstring url);

#endif