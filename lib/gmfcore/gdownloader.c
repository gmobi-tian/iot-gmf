#include "include/gdownloader.h"
#include "include/gfilestream.h"
#include "include/gfile.h"
GDownloader * g_downloader_new()
{
	GDownloader * downloader = g_new(GDownloader , 1);
	downloader->items = NULL;
	downloader->session = NULL;
	return downloader;
}

void g_downloader_item_free(GDownloaderItem * self)
{
	g_free(self->url);
	g_free(self->path);
	g_free(self);
}
void g_downloader_free(GDownloader * self)
{
	GList * items;
	g_return_if_fail(self != NULL);
	if (self->session)
		g_http_free(self->session);
	for(items = self->items; items != NULL; items = g_list_next(items))
		g_downloader_item_free((GDownloaderItem *)items->data);
	g_list_free(self->items);
	g_free(self);
}

static void g_downloader_on_status_changed(gpointer self, gpointer data, gpointer args);
static void g_downloader_on_finished(gpointer self, gpointer data, gpointer args) ;
static void g_downloader_process_task(GDownloader * self)
{
	if (self->items == NULL) return;
	if (self->session == NULL)
	{
		GDownloaderItem * di = (GDownloaderItem *)self->items->data;
		GHttpSession * s = self->session = g_http_new();
		
		if (di->resuming && g_file_exists(di->path))
		{
			s->downloaded_length = g_file_get_size(di->path);
			g_http_add_request_header(s, "Range", g_format("bytes=%d-", s->downloaded_length));
		}

		g_event_add_listener(s->event_on_status_changed, g_downloader_on_status_changed, di);
		g_event_add_listener(s->event_on_finished, g_downloader_on_finished, di);
		g_http_do_get(s, di->url);
	}
}
static void g_downloader_on_status_changed(gpointer self, gpointer data, gpointer args)
{
	GHttpSession* s = (GHttpSession*)self;
	GDownloaderItem* di = (GDownloaderItem*)data;

	switch(s->status)
	{
		case HTTP_STATUS_HEADER_RECEIVED:
			//if (g_http_has_content(s)) { *sometimes there are no content length*
				//gstring val = g_http_get_response_header(s, "location");
				s->response_stream = (GStream*)g_new_object(CLASS(GFileStream));
				//g_file_stream_open(s->response_stream, g_format("~%s.tmp", di->path), FILE_MODE_CREATE_NEW);
				if (di->resuming && g_file_exists(di->path))
				{
					g_file_stream_open(s->response_stream, di->path, FILE_MODE_READ_WRITE);
					g_file_stream_seek(s->response_stream, 0, SEEK_END);
				}
				else
				{
					g_file_stream_open(s->response_stream, di->path, FILE_MODE_CREATE_NEW);
				}
			//}
			break;
		case HTTP_STATUS_CONTENT_RECEIVING:
			if (di->downloading != NULL)
				di->downloading(s, di->url, di->path, di->data);
			break;		
	}
}


static void g_downloader_on_finished(gpointer self, gpointer data, gpointer args) 
{
	GHttpSession* s = (GHttpSession*)self;
	GDownloaderItem* di = (GDownloaderItem*)data;
	if (s->response_stream != NULL)
	{
		g_delete_object(s->response_stream);
		s->response_stream = NULL;
	}
	if (di->downloaded)
		di->downloaded(s, di->url, di->path, di->data);
	di->downloader->items = g_list_remove(di->downloader->items, di);
	g_http_free(s);
	di->downloader->session = NULL;
	g_downloader_process_task(di->downloader);
}
static GDownloaderItem * _add_download_item(GDownloader * self, gstring url, gstring path, gpointer data, DOWNLOADED_FUNC downloaded, DOWNLOADING_FUNC downloading, gboolean resuming)
{
	GDownloaderItem * di = g_new(GDownloaderItem , 1);
	g_file_make_full_directory_for_file(path);
	di->downloader = self;
	di->url = g_strdup(url);
	di->path = g_strdup(path);
	di->data = data;
	di->downloaded = downloaded;
	di->downloading = downloading;
	di->resuming = resuming;
	self->items = g_list_append(self->items, di);
	g_downloader_process_task(self);
	return di;
}
GDownloaderItem * g_downloader_add_task(GDownloader * self, gstring url, gstring path, gpointer data, DOWNLOADED_FUNC downloaded, DOWNLOADING_FUNC downloading)
{
	return _add_download_item(self, url, path, data, downloaded, downloading, FALSE);
}
GDownloaderItem * g_downloader_add_resuming_task(GDownloader * self, gstring url, gstring path, gpointer data, DOWNLOADED_FUNC downloaded, DOWNLOADING_FUNC downloading)
{
	return _add_download_item(self, url, path, data, downloaded, downloading, TRUE);
}
GDownloaderItem * g_downloader_get_task(GDownloader * self, gstring url)
{
	GList * item = self->items;
	for(; item != NULL; item = g_list_next(item))
	{
		GDownloaderItem * di = (GDownloaderItem *)item->data;
		if (g_strequal(di->url, url)) return di;
	}
	return NULL;
}
void g_downloader_remove_task(GDownloader * self, gstring url)
{
	GDownloaderItem * di = g_downloader_get_task(self, url);
	if (di == NULL || self->items == NULL) return;
	if (self->items->data == di && self->session != NULL)
	{
		g_http_free(self->session);
		self->session = NULL;
	}
	self->items = g_list_remove(self->items, di);
	g_downloader_item_free(di);
}