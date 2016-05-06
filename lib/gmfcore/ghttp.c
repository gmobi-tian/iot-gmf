#include "include/ghttp.h"
#include "include/glib.h"
#ifdef UNDER_MRE
#include "vmsock.h"
#include "vmtimer.h"
#endif
#include "include/gmemorystream.h"
#include "include/gcoroutine.h"

static gint http_income_traffic = 0;
static gint http_outcome_traffic = 0;
static gint domain_error_times = 0;

#ifdef UNDER_MRE
VMINT g_tcp_connect(const char* host, const VMINT port, const VMINT apn, void (*callback)(VMINT handle, VMINT event));
void g_tcp_close(VMINT handle);
#endif

#define HTTP_URL_PREFIX "http://"
#define HTTPS_URL_PREFIX "https://"

typedef enum
{
	HTTP_SSL_START,
	HTTP_SSL_HANDSHARK,
	HTTP_SSL_FINISH
} GHttpSslStatus ;

typedef struct  
{
	GString * text_line;
	gchar last_char;
	gboolean is_finished;
} GHttpTextLineParser;

typedef struct
{
	GHttpSession * session;
	gint size_to_read;
} GHttpBinaryBlockParser;

#define HTTP_BUFFER_LENGTH 1024 * 2
typedef struct
{
	GHttpSession base;
	//GTimer * timer;
	gint parse_status;
	gboolean lock;
	gint time_mark;
	gint header_len;
	gint last_read_len;
	//gpointer buffer_headers;
	//gpointer buffer_tcp;
	//gint buflen;
	GHttpTextLineParser * text_line_parser;
	GHttpBinaryBlockParser * binary_block_parser;

	GHttpSslStatus https_status;
	gpointer https_session;
}GRealHttpSession;

#define RPS_STATUS				1
#define RPS_HEADER				2
#define RPS_CONTENT				3
#define RPS_CHUNK_HEADER		4
#define RPS_CHUNK_CONTENT		5
#define RPS_CHUNK_CONTENT_END	6

#define CR '\xD'
#define LF '\xA'

static gstring jsession = NULL;

static GHashTable * http_global_request_headers = NULL;

static gstring http_global_apn_host = NULL;
static gint http_global_apn_port = 80;
static GHttpApn http_global_apn_type = HTTP_APN_NET;
void g_http_set_apn(gstring host, gint port, GHttpApn type);
gboolean g_http_retest_apn();
static void g_http_prepare(GHttpSession * s);
static void process_http_session();
void g_http_add_to_queue(GHttpSession * s);
static gboolean _setup_connection(GHttpSession * s, gint no);

gboolean g_http_retest_apn()
{
#if !defined(_WIN32) && !defined(UNDER_IOT)
	vm_apn_info_ext apn_info;
	memset(&apn_info, 0, sizeof(vm_apn_info_ext));
	vm_get_apn_info(&apn_info);
	g_log_debug("g_http_retest_apn proxy ip : %s", apn_info.proxy_ip);
	if (apn_info.proxy_ip == NULL || apn_info.proxy_ip[0] == '\0' || g_strequal("0.0.0.0", apn_info.proxy_ip) || apn_info.proxy_port <= 0)
	{
		if (http_global_apn_type != HTTP_APN_NET)
		{
			g_http_set_apn(NULL, 0, HTTP_APN_NET);
			return TRUE;
		}
	}
	else
	{
		if (http_global_apn_type != HTTP_APN_WAP || !g_strequal(apn_info.proxy_ip, http_global_apn_host) || apn_info.proxy_port != http_global_apn_port)
		{
			g_http_set_apn(apn_info.proxy_ip, apn_info.proxy_port, HTTP_APN_WAP);
			return TRUE;
		}
	}
#endif
	return FALSE;
}

static void g_http_get_full_url(gstring url, gstring * fullurl)
{
	if (g_strindexof(url, "://", 0) != -1 && !g_strstartwith(url, HTTP_URL_PREFIX) && !g_strstartwith(url, HTTPS_URL_PREFIX))
	{
		*fullurl = g_strdup(url);
		return;
	}
	url = g_strreplace(url, " ", "%20");
	if (!g_strstartwith(url, HTTP_URL_PREFIX) && !g_strstartwith(url, HTTPS_URL_PREFIX))
		*fullurl = g_strconcat(HTTP_URL_PREFIX, url, NULL);
	else
		*fullurl = g_strdup(url);
	g_free(url);
}
gint g_http_get_default_port(GHttpSession * s)
{
	return s->is_https_request ? 443 : 80;
}
void g_http_parse_url(GHttpSession * s, gstring * host, gint * port)
{
	gint pos1, pos2;
	gint httplen; 
	gstring url = s->url;

	s->is_https_request = g_strstartwith(url, HTTPS_URL_PREFIX);
	httplen = s->is_https_request ? strlen(HTTPS_URL_PREFIX) : strlen(HTTP_URL_PREFIX);
	pos1 = g_strindexof(url, "/", httplen);
	pos2 = g_strindexof(url, ":", httplen);
	if (pos1 == -1)
	{
		if (pos2 == -1)
		{
			*host = g_strsubstring(url, httplen, -1);
			*port = g_http_get_default_port(s);
		}
		else
		{
			gstring portstr = g_strsubstring(url, pos2 + 1, -1);
			*host = g_strsubstring(url, httplen, pos2 - httplen);
			*port = g_strtointeger(portstr);
			g_free(portstr);
		}
	}
	else if (pos1 > httplen)
	{
		if (pos2 < pos1 && pos2 > httplen)
		{
			gstring portstr = g_strsubstring(url, pos2 + 1, pos1 - pos2 - 1);
			*host = g_strsubstring(url, httplen, pos2 - httplen);
			*port = g_strtointeger(portstr);
			g_free(portstr);
		}
		else
		{
			*host = g_strsubstring(url, httplen, pos1 - httplen);
			*port = g_http_get_default_port(s);
		}
	}
}
static GHttpTextLineParser * g_http_text_line_parser_new()
{
	GHttpTextLineParser * parser = g_new0(GHttpTextLineParser, 1);
	parser->is_finished = FALSE;
	parser->last_char = '\0';
	parser->text_line = g_string_new("");
	return parser;
}
static void g_http_text_line_parser_free(GHttpTextLineParser * parser)
{
	g_string_free(parser->text_line);
	g_free(parser);
}

static long g_http_text_line_parser_parse(GHttpTextLineParser * parser, void * pBuffer, gint length)
{
	int i;
	char * szBuffer = (char*)pBuffer;

	if (parser->is_finished)
	{
		parser->is_finished = FALSE;
		parser->last_char = '\0';
		g_string_truncate(parser->text_line, 0);
	}

	for (i = 0; i < length && !parser->is_finished; parser->last_char = szBuffer[i], i++)
	{
	  switch(szBuffer[i])
	  {
	  case CR:
		  switch (parser->last_char)
		  {
		  case CR: i--;					
		  case LF: 
			  parser->is_finished = TRUE;
			  break;
		  }
		  break;
	  case LF:
		  switch (parser->last_char)
		  {
		  case LF: i--;
		  case CR: 
			  parser->is_finished = TRUE;
			  break;
		  }
		  break;
	  default:
		  switch (parser->last_char)
		  {
		  case LF: 
		  case CR: i--;
			  parser->is_finished = TRUE;
			  break;
		  }
		  if (!parser->is_finished) 
			  g_string_append_c(parser->text_line, szBuffer[i]);
		  break;
	  }
	}
	return parser->is_finished ? i : length;
}
static GHttpBinaryBlockParser * g_http_binary_block_parser_new(GHttpSession * s)
{
	GHttpBinaryBlockParser * parser = g_new0(GHttpBinaryBlockParser, 1);
	parser->size_to_read = 0;
	parser->session = s;
	return parser;
}
static void g_http_binary_block_parser_free(GHttpBinaryBlockParser * parser)
{
	g_free(parser);
}
static gint g_http_binary_block_parser_parse(GHttpBinaryBlockParser * parser, void * pBuffer, gint length)
{
	if (parser->size_to_read == 0) return 0;

	if (parser->size_to_read < length) 
		length = parser->size_to_read;
	if (parser->session->response_stream != NULL)
		g_stream_virtual_write(parser->session->response_stream, pBuffer, length);
	parser->session->downloaded_length += length;
	parser->size_to_read -= length;
	return length;
}

#define MAX_AVAILABLE_SESSIONS 3
static GHttpSession* processing_sessions[MAX_AVAILABLE_SESSIONS] = {0, };
static gpointer processing_sessions_buffer_tcp[MAX_AVAILABLE_SESSIONS] = {0, };
static gpointer processing_sessions_buffer_headers[MAX_AVAILABLE_SESSIONS] = {0, };

static gint processing_sessions_started_time[MAX_AVAILABLE_SESSIONS] = {0, };
static gboolean http_module_initialized = FALSE;
static gpointer http_module_process_timer = NULL;
static GList* session_queue_to_process = NULL;
static GList* session_queue_to_destroy = NULL;

gboolean g_http_is_waiting_for_destroying(GHttpSession * s)
{
	return g_list_index(session_queue_to_destroy, s) != -1;
}

void g_http_set_status(GHttpSession * s, GHttpStatus status)
{
	if (g_http_is_waiting_for_destroying(s))
		return;

	s->status = status;
	switch(status)
	{
	case HTTP_STATUS_CONNECTED:
		/*
		if (((GRealHttpSession*)s)->timer)
		{
			g_timer_destroy(((GRealHttpSession*)s)->timer);
			((GRealHttpSession*)s)->timer = NULL;
		}*/
		break;
	case HTTP_STATUS_CONTENT_RECEIVING:	
		break;
	case HTTP_STATUS_CONTENT_RECEIVED:	
		if (s->response_stream != NULL)
		{
			g_stream_virtual_flush(s->response_stream);
			g_stream_virtual_seek(s->response_stream, 0, SEEK_BEGIN);
		}
		break;

	}
	if (((GRealHttpSession*)s)->lock)
	{
		g_log_error("reenter set status !");
	}
	((GRealHttpSession*)s)->lock = TRUE;
	g_event_fire(s->event_on_status_changed, NULL);
	((GRealHttpSession*)s)->lock = FALSE;

	if (!g_http_is_waiting_for_destroying(s))
	{
		switch(status)
		{
		case HTTP_STATUS_HEADER_RECEIVED:	
			if (s->response_stream == NULL)
				s->response_stream = (GStream*)g_new_object(CLASS(GMemoryStream));
			if (s->response_stream != NULL && s->content_length > 0)
				g_stream_virtual_set_length(s->response_stream, s->content_length);
			//if Content-Length = 0, that means it is an empty response
			if (s->content_length == 0 && g_hash_table_lookup(s->response_headers, "Content-Length") != NULL)
				g_http_set_status(s, HTTP_STATUS_CONTENT_RECEIVED);			
			break;

		}
		if (status != HTTP_STATUS_CONTENT_RECEIVING)
		{
			process_http_session();
		}
	}
}

static void g_http_parse_received_buffer(int no, void * pBuffer, gint length) 
{
	GRealHttpSession * s = (GRealHttpSession*)processing_sessions[no];
	GHttpSession * hs = (GHttpSession *)s;
	char* http_buffer;
	char* http_header = (char*)processing_sessions_buffer_headers[no];
	int currPosition = 0;
	int i = 0;
	if (length <= 0 || s == NULL) return;
	while (currPosition < length)
	{
		int pos = 0;
		g_log_debug("parse status : [%d] %d/%d", s->parse_status, currPosition, length);
		if (s->parse_status == RPS_STATUS)
		{
			//LOG_DEBUG("RPS_STATUS");
			int len = length - currPosition;
			http_buffer = (char*)pBuffer + currPosition;
			for(i = 0; i < len; i++)
			{
				http_header[s->header_len++] = http_buffer[i];
				pos ++;
				if (s->header_len >= 2 && http_header[s->header_len - 1] == LF && http_header[s->header_len - 2] == CR)
				{
					gint pos1, pos2;
					gstring status;
					gint rc;
					http_header[s->header_len - 1] = '\0';
					pos1 = g_strindexof(http_header, " ", 0);
					pos2 = g_strindexof(http_header, " ", pos1 + 1);
					status = g_strsubstring(http_header, pos1 + 1, pos2 - pos1 - 1);
					g_log_debug("status : [%s]", status);
					hs->response_code = rc = g_strtointeger(status);
					hs->response_status = g_strsubstring(http_header, pos2 + 1, 0);
					s->parse_status = RPS_HEADER;
					g_free(status);
					s->header_len = 0;
					break;
				}
			}
		}
		else if (s->parse_status == RPS_HEADER)
		{
			//SetState(HTTP_STATE_RECEIVING_HEADER);

			int len = length - currPosition;
			http_buffer = (char*)pBuffer + currPosition;
			for(i = 0; i < len; i++)
			{
				pos ++;
				http_header[s->header_len++] = http_buffer[i];	
				if (s->header_len >= HTTP_BUFFER_LENGTH - 1)
				{
					s->header_len = 0;
					g_log_error("http buffer overflow !!!");
				}			
				if (s->header_len >= 2)
				{
					if (http_header[s->header_len - 1] == LF && http_header[s->header_len - 2] == CR)
					{
						http_header[s->header_len - 1] = '\0';
						if (s->header_len == 2) 
						{
							if (hs->is_chunked)
								s->parse_status = RPS_CHUNK_HEADER;
							else
							{
								s->parse_status = RPS_CONTENT;
								s->binary_block_parser->size_to_read = hs->content_length;
							}
							g_log_debug("res code:%d, bad=%d", hs->response_code, hs->raise_error_for_bad_response);
							if (hs->response_code >= 400 && hs->raise_error_for_bad_response)
							{
								g_http_set_status((GHttpSession*)s, HTTP_STATUS_ERROR);
								return;
							}
							else
							{
								g_http_set_status((GHttpSession*)s, HTTP_STATUS_HEADER_RECEIVED);
								break;
							}
						}
						else
						{
							gstring key, val;
							int pos = g_strindexof(http_header, ":", 0);
							if (pos == -1)
							{
								g_http_set_status((GHttpSession*)s, HTTP_STATUS_ERROR);
								return;
							}
							http_header[pos] = '\0';
							key = g_strtrim(http_header);
							val = g_strtrim(http_header + pos + 1);
							if (g_strcasecmp(key, "transfer-encoding") == 0)
							{
								hs->is_chunked = g_strcasecmp(val, "chunked") == 0;
							}					
							else if (g_strcasecmp(key, "content-range") == 0)
							{
								hs->content_length = g_strtointeger(strstr(val, "/") + 1);
								//hs->downloaded_length = g_string_parse_integer(val, '/', 10);
							}
							else if (g_strcasecmp(key, "content-length") == 0)
							{
								hs->session_content_length = g_strtointeger(val);	
								if (hs->content_length == 0)
									hs->content_length = hs->session_content_length;
							}
							g_log_debug("%s=%s", key, val);
							g_hash_table_insert(hs->response_headers, key, val);
							if (hs->delegate_on_header_received != NULL)
							{
								hs->delegate_on_header_received(hs, key, val);
								if ((VMINT)hs->handle == -1) return;
							}
						}

						s->header_len = 0;
					}
				}
			}
		}
		else if (s->parse_status == RPS_CONTENT)
		{
			/*
			if (hs->status == HTTP_STATUS_SENT)
				g_http_set_status((GHttpSession*)s, HTTP_STATUS_HEADER_RECEIVED);
			if (hs->content_length == 0)
			{
				g_stream_virtual_write(hs->response_stream,
					(char*)pBuffer + currPosition, 
					length - currPosition);
				pos = length - currPosition;
			}
			else
			{
				pos = g_http_binary_block_parser_parse(s->binary_block_parser,
					(char*)pBuffer + currPosition, 
					length - currPosition);
				if (s->binary_block_parser->size_to_read == 0)
				{
					g_http_set_status((GHttpSession*)s, HTTP_STATUS_CONTENT_RECEIVED);
					return;
				}
			}*/
			pos = length - currPosition;
			if (hs->content_length > 0 &&
				hs->downloaded_length + pos > hs->content_length)
			{
				pos = hs->content_length - hs->downloaded_length;
			}
			if (hs->response_stream != NULL)
				g_stream_virtual_write(hs->response_stream, (char*)pBuffer + currPosition, pos);
			hs->downloaded_length += pos;
			if (hs->content_length > 0 &&
				hs->downloaded_length >= hs->content_length)
			{
				g_log_debug("content downloaded (%d) = %d", hs->handle, hs->downloaded_length);
				g_http_set_status(hs, HTTP_STATUS_CONTENT_RECEIVED);
				return;
			}

		}
		else if (s->parse_status == RPS_CHUNK_HEADER)
		{
			if (hs->status == HTTP_STATUS_SENT)
				g_http_set_status((GHttpSession*)s, HTTP_STATUS_HEADER_RECEIVED);
			pos = g_http_text_line_parser_parse(s->text_line_parser,
				(char*)pBuffer + currPosition, 
				length - currPosition);
			if (s->text_line_parser->is_finished)
			{
				GString * hex = s->text_line_parser->text_line;
				gint size = g_string_parse_integer(hex, '\0', 16);
				if (size == 0)
				{
					g_http_set_status((GHttpSession*)s, HTTP_STATUS_CONTENT_RECEIVED);
					return;
				}
				else
				{
					s->binary_block_parser->size_to_read = size;
					s->parse_status = RPS_CHUNK_CONTENT;
				}
			}
		}
		else if (s->parse_status == RPS_CHUNK_CONTENT)
		{
			pos = g_http_binary_block_parser_parse(s->binary_block_parser,
				(char*)pBuffer + currPosition, 
				length - currPosition);
			if (s->binary_block_parser->size_to_read == 0)
				s->parse_status = RPS_CHUNK_CONTENT_END;
		}
		else if (s->parse_status == RPS_CHUNK_CONTENT_END)
		{
			pos = g_http_text_line_parser_parse(s->text_line_parser,
				(char*)pBuffer + currPosition, 
				length - currPosition);
			if (s->text_line_parser->is_finished) 
				s->parse_status = RPS_CHUNK_HEADER;
		}
		else
			break;
		currPosition += pos;
	}
	if (hs->status == HTTP_STATUS_HEADER_RECEIVED || 
		hs->status == HTTP_STATUS_CONTENT_RECEIVING)
	{
		g_http_set_status((GHttpSession*)s, HTTP_STATUS_CONTENT_RECEIVING);
	}
}
static void http_add_header_to_buffer(const gpointer key, const gpointer val, gpointer buf)
{
	g_string_append((GString*)buf, g_format("%s: %s\r\n", key, val));
	g_log_debug("http header : %s=%s", key, val);
}
static void http_add_param_to_buffer(const gpointer key, const gpointer val, gpointer buf)
{
	gstring sval = g_http_url_encode((gstring)val);
	g_string_append((GString*)buf, g_format("%s=%s&", key, sval));
	g_log_debug("http param : %s=%s", key, val);
	g_free(sval);
}
static int _tcp_read(GHttpSession * s, gpointer buffer, gint buflen)
{
	if (s->is_https_request)
	{
		GSslHandler * sh = g_https_get_ssl_handler();
		return sh->delegate_read(((GRealHttpSession*)s)->https_session, buffer, buflen);
	}
	else
	{
		return g_tcp_read((VMINT)s->handle, buffer, buflen);
	}
}

static int _tcp_write(GHttpSession * s, gpointer buffer, gint buflen)
{
	if (s->is_https_request)
	{
		GSslHandler * sh = g_https_get_ssl_handler();
		return sh->delegate_write(((GRealHttpSession*)s)->https_session, buffer, buflen);
	}
	else
	{
		return g_tcp_write((VMINT)s->handle, buffer, buflen);
	}
}
static gstring _get_path(gstring url)
{
	gint pos = g_strindexof(url, "://", 0);
	pos = g_strindexof(url, "/", pos == -1 ? 0 : pos + 3);
	return pos >= 0 ? url + pos : "/";
}
static void _tcp_callback(gint no, VMINT handle, VMINT event)
{ 
	GHttpSession * s = processing_sessions[no];
	gpointer buffer_tcp = processing_sessions_buffer_tcp[no];
	gpointer buffer_headers = processing_sessions_buffer_headers[no];
	gint buffer_length = HTTP_BUFFER_LENGTH;
	g_log_debug("_tcp_callback(%d,%d)", handle, event);

	if (s == NULL || s->handle == (ghandle) -1 || g_http_is_waiting_for_destroying(s)) return;

	
	((GRealHttpSession*)s)->time_mark = g_get_tick_count();

	if (s->is_https_request && ((GRealHttpSession*)s)->https_status != HTTP_SSL_FINISH)
	{
		GSslHandler * sh = g_https_get_ssl_handler();
		switch(event)
		{
		case VM_TCP_EVT_CONNECTED:
			g_http_set_status(s, HTTP_STATUS_CONNECTED);	
		case VM_TCP_EVT_CAN_WRITE:
			if (((GRealHttpSession*)s)->https_status == HTTP_SSL_START)
			{
				sh->delegate_sharkhand(((GRealHttpSession*)s)->https_session);
				((GRealHttpSession*)s)->https_status = HTTP_SSL_HANDSHARK;
			}
			break;
		case VM_TCP_EVT_CAN_READ:
			if (((GRealHttpSession*)s)->https_status == HTTP_SSL_HANDSHARK)
			{
				_tcp_read(s, buffer_tcp, buffer_length);
				if (sh->delegate_is_shakehand_over(((GRealHttpSession*)s)->https_session))
				{
					((GRealHttpSession*)s)->https_status = HTTP_SSL_FINISH;
					event = VM_TCP_EVT_CONNECTED;
					goto SSL_HANDSHAKE_SESSION_COMPLETED;
				}
			}
			break;
		default:
			goto SSL_HANDSHAKE_SESSION_COMPLETED;
		}
		return;
	}

SSL_HANDSHAKE_SESSION_COMPLETED:

    switch(event)
	{
	/* will trigger the message when tcp connect is created, can send and receive the data now. */
	case VM_TCP_EVT_CONNECTED:
		g_log_debug("connection created : %s", s->url);
		if (s->status != HTTP_STATUS_CONNECTED)
			g_http_set_status(s, HTTP_STATUS_CONNECTED);		
		{
			GString * buf = g_string_new("");
			GString * bufparams = NULL;
			gstring host;
			gstring path;
			gint port;
			gboolean direct = FALSE;
			VMINT len = -1;
			switch(s->method)
			{
			case HTTP_METHOD_GET: g_string_append(buf, "GET "); break;
			case HTTP_METHOD_POST: g_string_append(buf, "POST "); break;
			case HTTP_METHOD_DELETE: g_string_append(buf, "DELETE "); break;
			case HTTP_METHOD_PUT: g_string_append(buf, "PUT "); break;
			default: g_string_free(buf); return;
			}
			bufparams = g_string_new("");
			if (s->request_params)
			{
				if (s->method == HTTP_METHOD_GET || s->method == HTTP_METHOD_DELETE || s->method == HTTP_METHOD_PUT)
				{
					gint pos1, pos2 = -1;
					g_string_append(bufparams, _get_path(s->url));
					pos1 = g_strindexof(s->url, "?", 0);
					if (pos1 != -1)
					{
						if (bufparams->str[bufparams->len - 1] != '&')
							g_string_append(bufparams, "&");
					}
					else
						g_string_append(bufparams, "?");
				}
				else
				{
					if (g_hash_table_lookup(s->request_headers, "Content-Type") == NULL)
					{
						g_http_add_request_header(s, "Content-Type", "application/x-www-form-urlencoded");
					}
				}
				g_hash_table_foreach(s->request_params, http_add_param_to_buffer, bufparams);
				if (bufparams->len > 0)
				{
					if (bufparams->str[bufparams->len - 1] == '&')
					{
						bufparams->str[bufparams->len - 1] = '\0';
						bufparams->len--;
					}
				}
			}
			if (g_strendwith(s->url, ">>>"))
			{
				direct = TRUE;
				s->url[strlen(s->url) - 3] = '\0';
			}
			g_http_parse_url(s, &host, &port);
			path = (s->method == HTTP_METHOD_GET || s->method == HTTP_METHOD_DELETE || s->method == HTTP_METHOD_PUT)
				&& bufparams->len > 0 ? bufparams->str : s->url;
			if (!direct && jsession)
			{
				int pos = g_strindexof(path, "?", 0);
				if (pos == -1)
					path = g_strdup(g_format("%s;jsessionid=%s", path, jsession));
				else
				{
					gstring oldpath = path;
					char c = oldpath[pos];
					oldpath[pos] = '\0';
					path = g_strdup(g_format("%s;jsessionid=%s?%s", path, jsession, path + pos + 1));
					oldpath[pos] = c;
				}
			}
			if (port == g_http_get_default_port(s))
				g_string_append(buf, g_format("%s HTTP/1.1\r\nHost: %s\r\n", _get_path(path), host));
			else
				g_string_append(buf, g_format("%s HTTP/1.1\r\nHost: %s:%d\r\n", _get_path(path), host, port));
			if (!direct && jsession)
			{
				g_free(path);
			}
			if (http_global_request_headers != NULL && s->add_global_headers)
				g_hash_table_foreach(http_global_request_headers, http_add_header_to_buffer, buf);
			if ((s->method == HTTP_METHOD_POST || s->method == HTTP_METHOD_PUT) && s->request_stream == NULL)
				g_http_add_request_header(s, "Content-Type", "application/x-www-form-urlencoded");
			if (s->method == HTTP_METHOD_POST || s->method == HTTP_METHOD_PUT)
				g_http_add_request_header(s, "Content-Length", g_format("%d", s->request_stream != NULL ? g_stream_virtual_get_length(s->request_stream) : bufparams->len));
			g_hash_table_foreach(s->request_headers, http_add_header_to_buffer, buf);
			g_string_append(buf, "\r\n");
			g_log_debug("request sent:\r\n%s", buf->str);
			g_free(host);

			len = _tcp_write(s, (void*)buf->str, (VMINT)buf->len);
			http_outcome_traffic += buf->len;
			if (s->method == HTTP_METHOD_POST || s->method == HTTP_METHOD_PUT)
			{
				if (s->request_stream != NULL)
				{
					gpointer buffer = buffer_tcp;
					gint buflen = buffer_length;
					g_log_debug("http request_stream length = %d", g_stream_virtual_get_length(s->request_stream));
					g_stream_virtual_seek(s->request_stream, 0, SEEK_BEGIN);
					while((len = g_stream_virtual_read(s->request_stream, buffer, buflen)) > 0)
					{
						VMINT r = _tcp_write(s, buffer, (VMINT)len);	
						((char*)buffer)[len] = '\0';
						g_log_debug("%d : http writing %d/%d -> %s", s->handle, r, len, buffer);
						if (r == len)
						{							
							http_outcome_traffic += len;
						}
						else
						{
							g_stream_virtual_seek(s->request_stream, -len, SEEK_CURRENT);
							len = r;
							g_http_set_status(s, HTTP_STATUS_SENDING);
							break;
						}
					}
				}
				else
				{
					_tcp_write(s, (void*)bufparams->str, (VMINT)bufparams->len);
					http_outcome_traffic += bufparams->len;
				}
			}
			if (len > 0 || (len == 0 && s->status != HTTP_STATUS_SENDING))
			{
				g_log_debug("data sending successful\r\n");
				g_http_set_status(s, HTTP_STATUS_SENT);
				((GRealHttpSession*)s)->parse_status = RPS_STATUS;
				s->is_chunked = FALSE;
			}	
			else if (0 == len)
			{
				g_log_debug("write block");
			}
			else
			{
				g_log_debug("sending data failed");
			}
			g_string_free(buf);
			g_string_free(bufparams);
		}
		break;

	case VM_TCP_EVT_CAN_WRITE:
		{
			if (s->method == HTTP_METHOD_POST || s->method == HTTP_METHOD_PUT)
			{
				if (s->request_stream != NULL && !g_stream_virtual_is_eof(s->request_stream))
				{
					gpointer buffer = buffer_tcp;
					gint buflen = buffer_length;
					gint len;
					while((len = g_stream_virtual_read(s->request_stream, buffer, buflen)) > 0)
					{
						VMINT r = _tcp_write(s, buffer, (VMINT)len);	
						g_log_debug("%d >> http writing %d/%d", s->handle, r, len);
						if (r == len)
						{							
							http_outcome_traffic += len;
						}
						else
						{
							g_stream_virtual_seek(s->request_stream, -len, SEEK_CURRENT);
							len = r;
							break;
						}
					}
					if (len > 0 || g_stream_virtual_is_eof(s->request_stream))
					{
						g_log_debug("%d >> data sending successful", s->handle);
						g_http_set_status(s, HTTP_STATUS_SENT);
						((GRealHttpSession*)s)->parse_status = RPS_STATUS;
						s->is_chunked = FALSE;
					}	
					else if (0 == len)
					{
						g_log_debug("%d >> write block %d/%d",
							s->handle,
							g_stream_virtual_get_position(s->request_stream),
							g_stream_virtual_get_length(s->request_stream)
							);
					}
				}
			}
		}
		break;
	/**
	 * when tcp connection blocked, and Buffer has space to read, will trigger the message.
	 * can use vm_tcp_read to read the data, if vm_tcp_read returns 0, it can be
	 * parameter setting error or TCP disconnect£¬or not enough buffer space to read,
	 * can not read data at that time, need to wait for the next VM_TCP_EVT_CAN_READ to read again
	 * if vm_tcp_read return -1, data has been read end
	 */
    case VM_TCP_EVT_CAN_READ:
		{		

			char* http_buffer = buffer_tcp;
			gint http_buflen = buffer_length;			
			VMINT len = -1;
			gint total = 0;
			/*
			while ((VMINT)s->handle != -1 && (len = _tcp_read(s, http_buffer, http_buflen)) > 0)
			{	
				g_http_parse_received_buffer(no, http_buffer, len);
				total += len;
			}
			*/
			if ((VMINT)s->handle != -1 && (len = _tcp_read(s, http_buffer, http_buflen)) > 0)
			{	
				g_http_parse_received_buffer(no, http_buffer, len);
				total += len;
			}
			http_income_traffic += total;
			((GRealHttpSession*)s)->last_read_len = total;
			if (total >= 0)
			{
				g_log_debug("data read (%d)", total);
			}
			else if (VM_TCP_READ_EOF == len)
			{
				g_log_debug("data reach EOF (%d)", total);
				//if (s->status == HTTP_STATUS_CONTENT_RECEIVING)
				//	g_http_set_status(s, HTTP_STATUS_CONTENT_RECEIVED);
			}
			else
			{
				g_log_debug("data read failed %d", len);
				//g_http_set_status(s, HTTP_STATUS_ERROR);
			}
		}
		break;
	/* if can not reach the server will trigger the message, do not need to call vm_tcp_close to close the connect now */
	case VM_TCP_EVT_PIPE_BROKEN:
		{
			g_log_debug("can not connect to the server : %s", s->url);
			g_http_set_status(s, HTTP_STATUS_ERROR);
			//g_http_retest_apn();
		}
		break;
	/* if domain name analyze failed will trigger the message, do not need to call vm_tcp_close to close the connect now */
	case VM_TCP_EVT_HOST_NOT_FOUND:
		{
			g_log_debug("domain name analyze failed : %s", s->url);
			g_http_set_status(s, HTTP_STATUS_ERROR);
			if (http_income_traffic == 0)
			{
				g_http_retest_apn();
			}
		}
		break;		
	/* if server closed will trigger the message, do need to call vm_tcp_close to close the connection now */
	case VM_TCP_EVT_PIPE_CLOSED:
	    {
			g_log_debug("connection closed");
			//g_tcp_close((VMINT)s->handle);
			//s->handle = (ghandle)-1;
			if (s->status == HTTP_STATUS_CONTENT_RECEIVING)
				g_http_set_status(s, HTTP_STATUS_CONTENT_RECEIVED);
			else if (s->status != HTTP_STATUS_CONTENT_RECEIVED)
				g_http_set_status(s, HTTP_STATUS_ERROR);
	    }
		break;
	default:
		{
			g_log_debug("unknown event");
			g_tcp_close((VMINT)s->handle);
			s->handle = (ghandle)-1;
		}
		break;
	}
}

static void _tcp_callback_no0(VMINT handle, VMINT event)
{
	_tcp_callback(0, handle, event);
}
static void _tcp_callback_no1(VMINT handle, VMINT event)
{
	_tcp_callback(1, handle, event);
}
static void _tcp_callback_no2(VMINT handle, VMINT event)
{
	_tcp_callback(2, handle, event);
}
static void _tcp_callback_no3(VMINT handle, VMINT event)
{
	_tcp_callback(3, handle, event);
}
/*
static void	on_session_timer_triggered(GTimer * timer, gpointer data)
{
	GHttpSession * s = (GHttpSession*)data;
	g_http_set_status(s, HTTP_STATUS_TIMEOUT);
	g_timer_stop(timer);
}*/
static gboolean _setup_connection(GHttpSession * s, gint no)
{
	VMINT apn;
	g_http_prepare(s);
	if (g_strindexof(s->host, ".", 0) == -1)
	{
		s->max_retry_times = 0;
		return FALSE;
	}
#ifdef UNDER_MRE
	apn = s->apn == HTTP_APN_WAP ? VM_TCP_APN_CMWAP : s->apn == VM_TCP_APN_CMNET ? VM_TCP_APN_CMNET : VM_TCP_APN_WIFI;
	g_log_debug("setup connection %s:%d {%s} url:%s", s->host, s->port, apn == VM_TCP_APN_CMWAP ? "wap" : "net", s->url);
#endif
	switch(no)
	{
		case 0: s->handle = (ghandle)g_tcp_connect(s->host, s->port, apn, _tcp_callback_no0); break;
		case 1: s->handle = (ghandle)g_tcp_connect(s->host, s->port, apn, _tcp_callback_no1); break;
		case 2: s->handle = (ghandle)g_tcp_connect(s->host, s->port, apn, _tcp_callback_no2); break;
		case 3: s->handle = (ghandle)g_tcp_connect(s->host, s->port, apn, _tcp_callback_no3); break;
	}
	if (VM_TCP_ERR_NO_ENOUGH_RES == (VMINT)s->handle)
	{
		g_log_debug("TCP connecting failed - no enough resource\n");
		return FALSE;
	}
	else if ((VMINT)s->handle == VM_TCP_ERR_CREATE_FAILED)
	{
		g_log_debug("create tcp failed");
		return FALSE;
	}
	else
	{
		((GRealHttpSession*)s)->time_mark = g_get_tick_count();
		/*
		((GRealHttpSession*)s)->timer = g_timer_new(s->time_out * 1000);
		g_timer_start(((GRealHttpSession*)s)->timer);
		g_timer_add_listener(((GRealHttpSession*)s)->timer, on_session_timer_triggered, s);*/
		/*
		if (g_http_retest_apn())
		{
			g_tcp_close((VMINT)s->handle);
			s->handle = (ghandle)-1;
			return FALSE;
		}*/
		if (s->is_https_request)
		{
			GSslHandler * handler = g_https_get_ssl_handler();
			((GRealHttpSession*)s)->https_status = HTTP_SSL_START;
			((GRealHttpSession*)s)->https_session = handler->delegate_create(s);
		}
		g_log_debug("connecting, please wait");
		return TRUE;
	}
}
gboolean process_http_session_lock = FALSE;
gboolean process_http_session_enabled = TRUE;
static void process_http_session()
{
	int i;
	GHttpSession* s;
	gboolean need_to_process_again = FALSE;
	if (!process_http_session_enabled) return;
	if (process_http_session_lock) return;
	process_http_session_lock = TRUE;
	while(session_queue_to_destroy != NULL)
	{
		s = (GHttpSession*)session_queue_to_destroy->data;	
		if (((GRealHttpSession*)s)->lock) break;
		session_queue_to_destroy = g_list_remove(session_queue_to_destroy, s);
		g_http_free(s);
	}
	for(i = 0; i < MAX_AVAILABLE_SESSIONS; i++)
	{
		if (processing_sessions[i] == NULL)
		{
			if (session_queue_to_process == NULL) continue;
			s = (GHttpSession*)session_queue_to_process->data;
			if (_setup_connection(s, i))
			{
				session_queue_to_process = g_list_remove(session_queue_to_process, s);
				processing_sessions[i] = s;
				processing_sessions_started_time[i] = g_get_tick_count();
#ifdef UNDER_IOT
                if ( s->handle >=0 ) _tcp_callback(i, (gint) s->handle, VM_TCP_EVT_CONNECTED);
#endif        
			}
			else
			{
				if (s->max_retry_times > 0)
				{
					s->max_retry_times --;
				}
				else
				{
					session_queue_to_process = g_list_remove(session_queue_to_process, s);
					g_http_set_status(s, HTTP_STATUS_ERROR);
					((GRealHttpSession*)s)->lock = TRUE;
					g_event_fire(s->event_on_finished, NULL);
					((GRealHttpSession*)s)->lock = FALSE;
				}
			}
		}
		else
		{
			s = processing_sessions[i];
			switch (s->status)
			{
			case HTTP_STATUS_CONNECTED:
				//_tcp_callback(s, (VMINT)s->handle, VM_TCP_EVT_CAN_WRITE);
#ifdef UNDER_IOT      
                _tcp_callback(i, (gint)s->handle, VM_TCP_EVT_CAN_WRITE);
#endif      
				break;
			case HTTP_STATUS_SENDING:				
				break;
			case HTTP_STATUS_SENT:
				//_tcp_callback(s, (VMINT)s->handle, VM_TCP_EVT_CAN_READ);
#ifdef UNDER_IOT       
                _tcp_callback(i, (gint)s->handle, VM_TCP_EVT_CAN_READ);
#endif      
				break;
			case HTTP_STATUS_CONTENT_RECEIVED:
			case HTTP_STATUS_ERROR:
			case HTTP_STATUS_TIMEOUT:
				if (!((GRealHttpSession*)s)->lock)
				{
					processing_sessions[i] = NULL;
					((GRealHttpSession*)s)->lock = TRUE;
					g_event_fire(s->event_on_finished, NULL);
					((GRealHttpSession*)s)->lock = FALSE;
					//g_http_close(s);
#ifdef UNDER_IOT 
                    g_http_close(s);
#endif          
					need_to_process_again = TRUE;
				}
				break;
			}
		}
	}
	process_http_session_lock = FALSE;
#ifdef UNDER_IOT   
	if (need_to_process_again) 
		process_http_session();
#endif  
	//if (need_to_process_again) 
	//	process_http_session();
}
void g_http_process_queue(gboolean enable)
{
	if (!enable)
		process_http_session();
	process_http_session_enabled = enable;
	if (enable)
		process_http_session();
}
void g_http_set_apn(gstring host, gint port, GHttpApn type)
{
	if (type == HTTP_APN_WAP)
		g_log_debug("g_http_set_apn {wap} %s:%d", host, port);
	else
		g_log_debug("g_http_set_apn {net}");
	http_global_apn_host = g_strdup(host);
	http_global_apn_port = port;
	http_global_apn_type = type;
}

static void on_process_http_session(VMINT tid)
{
	int i;
	if (process_http_session_lock) return;
	//g_log_debug("on_process_http_session %d", tid);
	process_http_session_lock = TRUE;
	for(i = 0; i < MAX_AVAILABLE_SESSIONS; i++)
	{
		if (processing_sessions[i] != NULL)
		{
			GHttpSession * s = processing_sessions[i];
			if (((GRealHttpSession*)s)->last_read_len > 0)
			{
				_tcp_callback(i, (VMINT)s->handle, VM_TCP_EVT_CAN_READ);
			}
			if (s->status == HTTP_STATUS_CONTENT_RECEIVING)
			{
				if (s->session_content_length == 0 && s->content_length > 0) // resume a download from the end of the file, just mark the status as received
				{
					g_http_set_status(s, HTTP_STATUS_CONTENT_RECEIVED);
				}
			}
			if (s->status < HTTP_STATUS_CONTENT_RECEIVED)
			{
				int tc = g_get_tick_count();
				int tm = ((GRealHttpSession*)s)->time_mark;
				if (s->time_out * 1000 < tc - tm)
					g_http_set_status(s, HTTP_STATUS_TIMEOUT);
			}
		}
	}		
	process_http_session_lock = FALSE;
	//if (session_queue_to_process == NULL) continue;
	process_http_session();
}
/*
static GCoroutineStatus on_process_http_session(GCoroutine* co)
{
	int i;
	g_coroutine_begin(co);
	while(1)
	{
		g_coroutine_sleep(co, 100);
		if (process_http_session_lock) continue;
		process_http_session_lock = TRUE;
		for(i = 0; i < MAX_AVAILABLE_SESSIONS; i++)
		{
			if (processing_sessions[i] != NULL)
			{
				GHttpSession * s = processing_sessions[i];
				if (s->status == HTTP_STATUS_CONTENT_RECEIVING)
				{
					if (s->session_content_length == 0 && s->content_length > 0) // resume a download from the end of the file, just mark the status as received
					{
						g_http_set_status(s, HTTP_STATUS_CONTENT_RECEIVED);
					}
				}
				if (s->status < HTTP_STATUS_CONTENT_RECEIVED)
				{
					int tc = g_get_tick_count();
					int tm = ((GRealHttpSession*)s)->time_mark;
					if (s->time_out * 1000 < tc - tm)
						g_http_set_status(s, HTTP_STATUS_TIMEOUT);
				}
			}
		}		
		process_http_session_lock = FALSE;
		//if (session_queue_to_process == NULL) continue;
		process_http_session();
	}
	g_coroutine_end(co);
}
*/

void g_http_module_initialize(gboolean testapn)
{
	int i;
	if (!http_module_initialized)
	{
#if !defined(_WIN32) && !defined(UNDER_IOT)
		vm_apn_info_ext apn_info;
		vm_get_apn_info(&apn_info);
		g_log_debug("g_http_module_initialize proxy ip : %s", apn_info.proxy_ip);

		if (testapn)
		{
			if (apn_info.proxy_ip == NULL || apn_info.proxy_ip[0] == '\0' || g_strequal("0.0.0.0", apn_info.proxy_ip))
				g_http_set_apn(NULL, 0, HTTP_APN_NET);
			else
				g_http_set_apn(apn_info.proxy_ip, apn_info.proxy_port, HTTP_APN_WAP);
		}		
		else
		{
			g_http_set_apn(NULL, 0, HTTP_APN_NET);
		}
#endif
		http_module_initialized = TRUE;
		process_http_session_enabled = TRUE;
		for(i = 0; i < MAX_AVAILABLE_SESSIONS; i++)
		{
			processing_sessions[i] = NULL;
			processing_sessions_buffer_tcp[i] = g_malloc(HTTP_BUFFER_LENGTH + 1);
			((char*)processing_sessions_buffer_tcp[i])[HTTP_BUFFER_LENGTH] = 0;
			processing_sessions_buffer_headers[i] = g_malloc(HTTP_BUFFER_LENGTH + 1);
			((char*)processing_sessions_buffer_headers[i])[HTTP_BUFFER_LENGTH] = 0;
			processing_sessions_started_time[i] = 0;
		}
		
		//g_coroutine_run(g_coroutine_new(), on_process_http_session);

		//g_http_add_global_request_header("Host", "localhost");
		
#if 0
		g_http_add_global_request_header("User-Agent", "*/*");
		g_http_add_global_request_header("Accept", "*/*");
		g_http_add_global_request_header("Accept-Language", "en-us,en;q=0.5");
		g_http_add_global_request_header("Accept-Charset", "UTF-8,ISO-8859-1,US-ASCII,ISO-10646-UCS-2;q=0.6");
		g_http_add_global_request_header("Accept-Encoding", "identity");
		g_http_add_global_request_header("Connection", "close");		
#endif	
	}

	if (http_module_process_timer == NULL)
	{
		http_module_process_timer = g_create_timer(100, on_process_http_session);
		g_log_debug("http_module_process_timer : %d", *((int *) http_module_process_timer));
	}

}
void static_clean_http()
{
	if (http_module_initialized)
	{
		http_module_initialized = FALSE;
		//vm_delete_timer(http_module_process_timer);
		if (session_queue_to_process != NULL)
			g_list_free(session_queue_to_process);
		if (session_queue_to_destroy != NULL)
			g_list_free(session_queue_to_destroy);
		if (http_global_request_headers != NULL)
			g_hash_table_deep_destroy(http_global_request_headers);
	}
}
GHttpSession * g_http_new()
{
	GRealHttpSession * session = g_new0(GRealHttpSession, 1);
	GHttpSession *s = (GHttpSession*)session;
	g_http_module_initialize(TRUE);
	session->lock = FALSE;
	session->time_mark = 0;
	session->header_len = 0;
	session->last_read_len = 0;
	session->text_line_parser = g_http_text_line_parser_new();
	session->binary_block_parser = g_http_binary_block_parser_new(s);
	//session->timer = NULL;
	s->retry = 3;
	s->handle = (ghandle)-1;
	s->host = NULL;
	s->port = 0;
	s->url = NULL;
	s->apn = HTTP_APN_NET;
	s->time_out = 60;
	s->priority = 0;
	s->add_global_headers = TRUE;
	s->raise_error_for_bad_response = TRUE;
	s->data = NULL;
	s->status = HTTP_STATUS_CREATED;
	s->event_on_status_changed = g_event_new(s);
	s->event_on_finished = g_event_new(s);
	s->request_headers = g_hash_table_new_with_string_key();
	s->request_params = NULL;
	s->request_stream = NULL;
	s->response_headers = g_hash_table_new(g_str_ihash, g_str_iequal);
	s->response_code = 0;
	s->response_status = NULL;
	s->response_stream = NULL;
	s->content_length = 0;
	s->session_content_length = 0;
	s->downloaded_length = 0;
	s->max_retry_times = 10;
	s->is_https_request = FALSE;
	s->delegate_on_header_received = NULL;
	return (GHttpSession *)session;
}
void g_http_delay_free(GHttpSession * s)
{
	if (g_list_index(session_queue_to_destroy, s) == -1)
		session_queue_to_destroy = g_list_append(session_queue_to_destroy, s);
}
void g_http_free(GHttpSession * s)
{
	GRealHttpSession * session = (GRealHttpSession *)s;	
	g_http_close(s);
	if (session->lock)
	{
		g_http_delay_free(s);
		return;
	}
	if (s->url)
	{
		g_free(s->url);
		s->url = NULL;
	}
	if (s->host)
	{
		g_free(s->host);
		s->host = NULL;
	}
	if (s->response_status){
		g_free(s->response_status);
		s->response_status = NULL;
	}
	if (s->is_https_request && ((GRealHttpSession*)s)->https_session)
	{
		GSslHandler * sh = g_https_get_ssl_handler();
		sh->delegate_destroy(((GRealHttpSession*)s)->https_session);
	}
	g_event_free(s->event_on_finished);
	g_event_free(s->event_on_status_changed);
	g_http_text_line_parser_free(session->text_line_parser);
	g_http_binary_block_parser_free(session->binary_block_parser);
	g_delete_object(s->response_stream);
	g_delete_object(s->request_stream);
	g_hash_table_deep_destroy(s->request_headers);
	g_hash_table_deep_destroy(s->request_params);
	g_hash_table_deep_destroy(s->response_headers);	
	g_free(s);
}
void g_http_close(GHttpSession * s)
{
	gint i;
	if (g_list_index(session_queue_to_process, s) != -1)
		session_queue_to_process = g_list_remove(session_queue_to_process, s);
	for(i = 0; i < MAX_AVAILABLE_SESSIONS; i++)
	{
		if (processing_sessions[i] == s)
		{
			processing_sessions[i]  = NULL;
			break;
		}
	}
	/*
	if (((GRealHttpSession *)s)->timer != NULL)
	{
		g_timer_destroy(((GRealHttpSession *)s)->timer);
		((GRealHttpSession *)s)->timer = NULL;
	}*/
	if (s->handle != (ghandle)-1)
	{	
		g_tcp_close((VMINT)s->handle);
		s->handle = (ghandle)-1;
	}
	//process_http_session();
}
gstring g_http_get_response_header(GHttpSession * s, const gstring name)
{
	return (gstring)g_hash_table_lookup(s->response_headers, name);
}

static gint _http_priority_compare_two (gconstpointer a, gconstpointer b)
{
	return ((const GHttpSession*)a)->priority - ((const GHttpSession*)b)->priority;
}

void g_http_add_to_queue(GHttpSession * s)
{
	if (g_list_index(session_queue_to_process, s) == -1)
	{
		session_queue_to_process = g_list_append(session_queue_to_process, s);
		//session_queue_to_process = g_list_insert_sorted(session_queue_to_process, s, _http_priority_compare_two);
		g_http_set_status(s, HTTP_STATUS_WAITING);
	}
}
void g_http_add_request_header(GHttpSession * s, gstring key, gstring val)
{
	gpointer okey = NULL, oval;
	g_hash_table_lookup_extended(s->request_headers, key, &okey, &oval);
	if (okey != NULL)
	{
		g_hash_table_remove(s->request_headers, key);
		g_free(okey);
		g_free(oval);
	}
	g_hash_table_insert(s->request_headers, g_strdup(key), g_strdup(val));
}

void g_http_add_global_request_header(gstring key, gstring val)
{
	gpointer okey = NULL, oval;
	if (http_global_request_headers == NULL)
		http_global_request_headers = g_hash_table_new_with_string_key();
	g_hash_table_lookup_extended(http_global_request_headers, key, &okey, &oval);
	if (okey != NULL)
	{
		g_hash_table_remove(http_global_request_headers, key);
		g_free(okey);
		g_free(oval);
	}
	g_hash_table_insert(http_global_request_headers, g_strdup(key), g_strdup(val));
}
static void http_copy_request_params(const gpointer key, const gpointer val, gpointer data)
{
	g_hash_table_insert((GHashTable*)data, g_strdup((const gchar*)key), g_strdup((const gchar*)val));
}

static void g_http_prepare(GHttpSession * s)
{
	s->apn = http_global_apn_type;
	if (s->host)
	{
		g_free(s->host);
		s->host = NULL;
	}
	if (http_global_apn_host != NULL)
	{
		s->host = g_strdup(http_global_apn_host);
		s->port = http_global_apn_port;
	}
	if (s->host == NULL)
		g_http_parse_url(s, &s->host, &s->port);
}

void g_http_do_fetch(GHttpSession * s, GHttpMethod method, gstring url, GHashTable * params, GStream * stream)
{
	if (s->status != HTTP_STATUS_CREATED) return;
	s->method = method;
	if (s->request_params)
	{
		g_hash_table_deep_destroy(s->request_params);
		s->request_params = NULL;
	}
	if (params)
	{
		s->request_params = g_hash_table_new_with_string_key();
		g_hash_table_foreach(params, http_copy_request_params, s->request_params);
	}
	s->request_stream = stream;
	g_http_get_full_url(url, &s->url);
	g_http_add_to_queue(s);
}
gboolean g_http_has_content(GHttpSession * s)
{
	if (s->response_stream != NULL && g_stream_virtual_get_length(s->response_stream) > 0)
		return TRUE;
	return s->content_length > 0 || s->is_chunked;
}
void g_http_do_get(GHttpSession * s, gstring url)
{
	g_http_do_fetch(s, HTTP_METHOD_GET, url, NULL, NULL);
}

void g_http_do_get_with_params(GHttpSession * s, gstring url, GHashTable * params)
{
	g_http_do_fetch(s, HTTP_METHOD_GET, url, params, NULL);
}
void g_http_do_post(GHttpSession * s, gstring url, GHashTable * params)
{
	g_http_do_fetch(s, HTTP_METHOD_POST, url, params, NULL);
}
void g_http_do_post_with_stream(GHttpSession * s, gstring url, GStream * stream)
{
	g_http_do_fetch(s, HTTP_METHOD_POST, url, NULL, stream);
}

void g_http_set_jsession(gstring js)
{
	if (jsession)
		g_free(jsession);
	jsession = g_strdup(js);
}
void g_http_get_traffic(gint * income, gint *outcome)
{
	*income = http_income_traffic;
	*outcome = http_outcome_traffic;
}
void g_http_reset_traffic()
{
	http_income_traffic = 0;
	http_outcome_traffic = 0;
}

static GSslHandler * global_ssl_handler = NULL;
void g_https_set_ssl_handler(GSslHandler * handler)
{
	global_ssl_handler = handler;
}
GSslHandler * g_https_get_ssl_handler()
{
	return global_ssl_handler;
}
char to_hex(char code) {
	static char hex[] = "0123456789ABCDEF";
	return hex[code & 15];
}

gchar from_hex(gchar ch) 
{
	return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

gstring g_http_url_decode(gstring str) 
{
	gchar *pstr = str, *buf = g_malloc(strlen(str) + 1), *pbuf = buf;
	while (*pstr) 
	{
		if (*pstr == '%') 
		{
			if (pstr[1] && pstr[2])
			{
				*pbuf++ = from_hex(pstr[1]) << 4 | from_hex(pstr[2]);
				pstr += 2;
			}
		} 
		else if (*pstr == '+') 
		{ 
			*pbuf++ = ' ';
		} 
		else 
		{
			*pbuf++ = *pstr;
		}
		pstr++;
	}
	*pbuf = '\0';
	return buf;
}

gstring g_http_url_encode(gstring str)
{
	char *pstr = str;
	char *buf = g_malloc(strlen(str) * 3 + 1);
	char *pbuf = buf;
	if (!pbuf) return str;

	while (*pstr) 
	{
		char ch = *pstr;
		if (((ch >= 0x30) &&(ch <= 0x39)) || 
			((ch >= 0x41) &&(ch <= 0x5a)) || ((ch >= 0x61) &&(ch <= 0x7a)) || 
			ch == '-' || ch == '_' || ch == '.' || ch == '~') 
			*pbuf++ = ch;
		else if (*pstr == ' ') 
			*pbuf++ = '+';
		else 
		{
			*pbuf++ = '%';
			*pbuf++ = to_hex(ch >> 4);
			*pbuf++ = to_hex(ch & 15);
		}
		pstr++;
	}
	*pbuf = '\0';
	return buf;
}
gstring g_http_get_encoded_url(gstring url)
{
	gstring nurl = NULL;
	g_http_get_full_url(url, &nurl);
	return nurl;
}

gboolean g_http_is_cacheable(GHttpSession * s)
{
	gstring val = (gstring)g_hash_table_lookup(s->response_headers, "Cache-Control");
	if (val == NULL) return TRUE;
	return !(g_strequal(val, "no-cache") || g_strequal(val, "no-store"));
}
