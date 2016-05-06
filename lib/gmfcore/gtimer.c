#include "include/glib.h"
#ifdef UNDER_MRE
#include "vmtimer.h"
#endif

static GHashTable * all_timers = NULL;
static GHashTable * all_extimers = NULL;
static gboolean mark_current_timer_to_free = FALSE;
GTimer * current_timer = NULL;

void static_clean_gtimer()
{
	if (all_timers != NULL)
		g_hash_table_destroy(all_timers);
	if (all_extimers != NULL)
		g_hash_table_destroy(all_extimers);
}
void _handle_timer_process(gint tid)
{
	if (all_timers != NULL)
	{
		GTimer * timer = (GTimer *)g_hash_table_lookup(all_timers, GUINT_TO_POINTER(tid));
		if (timer != NULL)
		{
			if (timer->callback_list != NULL)
			{
				GList *callback_list, *data_list;
				callback_list = timer->callback_list;
				data_list = timer->data_list;
				current_timer = timer;
				mark_current_timer_to_free = FALSE;
				for(; callback_list != NULL; callback_list = g_list_next(callback_list), data_list = g_list_next(data_list))
				{
					GTimerCallback callback = (GTimerCallback)callback_list->data;
					if (callback != NULL)
						callback(timer, data_list->data);
				}
				current_timer = NULL;
				if (mark_current_timer_to_free)
					g_timer_destroy(timer);
			}
		}
	}
}

void _handle_extimer_process(gint tid)
{
	if (all_extimers != NULL)
	{
		GTimer * timer = (GTimer *)g_hash_table_lookup(all_extimers, GUINT_TO_POINTER(tid));
		if (timer != NULL)
		{
			if (timer->callback_list != NULL)
			{
				GList *callback_list, *data_list;
				callback_list = timer->callback_list;
				data_list = timer->data_list;
				current_timer = timer;
				mark_current_timer_to_free = FALSE;
				for(; callback_list != NULL; callback_list = g_list_next(callback_list), data_list = g_list_next(data_list))
				{
					GTimerCallback callback = (GTimerCallback)callback_list->data;
					if (callback != NULL)
						callback(timer, data_list->data);
				}
				current_timer = NULL;
				if (mark_current_timer_to_free)
					g_timer_destroy(timer);
			}
		}
	}
}

GTimer * g_timer_new_ex(gint interval, gboolean background)
{
	GTimer * timer = g_new(GTimer, 1);
	timer->callback_list = NULL;
	timer->data_list = NULL;
	timer->enabled = FALSE;
	timer->handle = NULL;
	timer->interval = interval;
	timer->support_background_running = background;
	if (all_timers == NULL)
		all_timers = g_hash_table_new(g_direct_hash, g_direct_equal);
	if (all_extimers == NULL)
		all_extimers = g_hash_table_new(g_direct_hash, g_direct_equal);
	return timer;
}
GTimer * g_timer_new(gint interval)
{
	return g_timer_new_ex(interval, FALSE);
}
void g_timer_destroy(GTimer * timer)
{
	if (timer == current_timer)
	{
		mark_current_timer_to_free = TRUE;
		return;
	}
	g_return_if_fail(timer != NULL);
	g_timer_stop(timer);
	if (timer->callback_list != NULL)
	{
		g_list_free(timer->callback_list);
		timer->callback_list = NULL;
	}
	if (timer->data_list != NULL)
	{
		g_list_free(timer->data_list);
		timer->data_list = NULL;
	}
	g_free(timer);
}

gboolean g_timer_is_enabled(GTimer * timer)
{
	g_return_val_if_fail(timer != NULL, FALSE);
	return timer->enabled;
}
void g_timer_start(GTimer * timer)
{
	g_return_if_fail(timer != NULL);
	g_timer_stop(timer);
	timer->handle = timer->support_background_running ?
		(ghandle)g_create_timer_ex(timer->interval, _handle_extimer_process) :
	(ghandle)g_create_timer(timer->interval, _handle_timer_process);
	g_log_debug("g_timer_start timer %d %s", timer->handle, timer->support_background_running ? "b" : "f");
	if ((gint)timer->handle > 0)
	{
		timer->enabled = TRUE;
		if (timer->support_background_running)
			g_hash_table_insert(all_extimers, timer->handle, timer);
		else
			g_hash_table_insert(all_timers, timer->handle, timer);
	}
	else
	{
		g_log_error("fails to create timer!");
	}
}
void g_timer_stop(GTimer * timer)
{
	g_return_if_fail(timer != NULL);
	if (timer->handle != NULL)
	{
#ifdef UNDER_MRE
		if (timer->support_background_running)
			vm_delete_timer_ex((gint)timer->handle);
		else
			vm_delete_timer((gint)timer->handle);
		g_log_debug("g_timer_stop timer %d %s", timer->handle, timer->support_background_running ? "b" : "f");
#endif
		if (timer->support_background_running)
			g_hash_table_remove(all_extimers, timer->handle);
		else
			g_hash_table_remove(all_timers, timer->handle);
		timer->handle = NULL;
	}
	timer->enabled = FALSE;
}
void g_timer_add_listener(GTimer * timer, GTimerCallback callback, gpointer data)
{
	GList *callback_list, *data_list;
	g_return_if_fail(timer != NULL);
	callback_list = timer->callback_list;
	data_list = timer->data_list;
	for(; callback_list != NULL; callback_list = g_list_next(callback_list), data_list = g_list_next(data_list))
	{
		if (callback_list->data == (gpointer)callback && data_list->data == data) return;
	}
	timer->callback_list = g_list_append(timer->callback_list, (gpointer)callback);
	timer->data_list = g_list_append(timer->data_list, data);
}
void g_timer_remove_listener(GTimer * timer, GTimerCallback callback, gpointer data)
{	
	GList *callback_list, *data_list;
	g_return_if_fail(timer != NULL);
	callback_list = timer->callback_list;
	data_list = timer->data_list;
	for(; callback_list != NULL; callback_list = g_list_next(callback_list), data_list = g_list_next(data_list))
	{
		if (callback_list->data == (gpointer)callback && data_list->data == data)
		{
			timer->callback_list = g_list_remove_link(timer->callback_list, (gpointer)callback_list);
			timer->data_list = g_list_remove_link(timer->data_list, data_list);
			break;
		}
	}
}
