#ifndef _G_HTTP_H_
#define _G_HTTP_H_
#include "gevent.h"
#include "gstream.h"

typedef struct _GSslHandler
{
	gpointer (*delegate_create)(gpointer session);
	void (*delegate_destroy)(gpointer self);
	int (*delegate_read)(gpointer self, gpointer buffer, gint buflen);
	int (*delegate_write)(gpointer self, gpointer buffer, gint buflen);
	int (*delegate_is_shakehand_over)(gpointer self);
	void (*delegate_sharkhand)(gpointer self);
} GSslHandler;

typedef struct _GHttpSession GHttpSession;
typedef enum
{
	HTTP_METHOD_GET,
	HTTP_METHOD_POST,
	HTTP_METHOD_PUT,
	HTTP_METHOD_DELETE
}GHttpMethod;

typedef enum
{
	HTTP_APN_WAP,
	HTTP_APN_NET,
	HTTP_APN_WIFI
}GHttpApn;

typedef enum 
{
	HTTP_STATUS_CREATED,
	HTTP_STATUS_WAITING,
	HTTP_STATUS_CONNECTED,
	HTTP_STATUS_SENDING,
	HTTP_STATUS_SENT,
	HTTP_STATUS_HEADER_RECEIVED,
	HTTP_STATUS_CONTENT_RECEIVING,
	HTTP_STATUS_CONTENT_RECEIVED,
	HTTP_STATUS_ERROR,
	HTTP_STATUS_TIMEOUT
}GHttpStatus;

struct _GHttpSession
{
	GHttpApn apn;
	gstring url;
	gstring host;
	gint port;
	GHttpMethod method;
	GHttpStatus status;
	gint time_out; // seconds
	gint max_retry_times;
	gint priority; 
	gboolean add_global_headers;
	gpointer data;

	GStream * request_stream;
	GStream * response_stream;
	GHashTable * request_headers;
	GHashTable * request_params;
	GHashTable * response_headers;
	gint retry;
	gboolean is_chunked;

	gboolean is_https_request;
	gboolean raise_error_for_bad_response;  // HTTP response code >= 400

	gint response_code;
	gint content_length;
	gint session_content_length;
	gint downloaded_length;
	gstring response_status;
	ghandle handle;

	GEvent * event_on_status_changed;
	GEvent * event_on_finished;
	
	void (*delegate_on_header_received)(GHttpSession * self, gstring key, gstring val);
}; 

void g_http_module_initialize(gboolean testapn);
GHttpSession * g_http_new(void);
gboolean g_http_has_content(GHttpSession * s);
void g_http_delay_free(GHttpSession * s);
void g_http_free(GHttpSession * s);
void g_http_close(GHttpSession * s);
gboolean g_http_retest_apn(void);
//void g_http_set_apn(gstring host, gint port, GHttpApn apn);
//void g_http_connect(GHttpSession * s, gstring host, gint port, GHttpApn apn);
void g_http_do_fetch(GHttpSession * s, GHttpMethod method, gstring url, GHashTable * params, GStream * stream);
void g_http_do_get(GHttpSession * s, gstring url);
void g_http_do_get_with_params(GHttpSession * s, gstring url, GHashTable * params);
void g_http_do_post(GHttpSession * s, gstring url, GHashTable * params);
void g_http_do_post_with_stream(GHttpSession * s, gstring url, GStream * stream);
void g_http_add_request_header(GHttpSession * s, gstring key, gstring val);
void g_http_add_global_request_header(gstring key, gstring val);
void g_http_set_jsession(gstring js);
void g_http_process_queue(gboolean enable);
gstring g_http_get_response_header(GHttpSession * s, const gstring name);

GSslHandler * g_https_get_ssl_handler(void);
void g_https_set_ssl_handler(GSslHandler * handler);

void g_http_get_traffic(gint * income, gint * outcome);
void g_http_reset_traffic(void);

gstring g_http_get_encoded_url(gstring url);

gstring g_http_url_encode(gstring str);
gstring g_http_url_decode(gstring str);

gboolean g_http_is_cacheable(GHttpSession * s);

#endif
