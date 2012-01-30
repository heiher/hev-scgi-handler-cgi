/*
 ============================================================================
 Name        : hev-scgi-handler-cgi.c
 Author      : Heiher <admin@heiher.info>
 Version     : 0.0.1
 Copyright   : Copyright (C) 2011 everyone.
 Description : 
 ============================================================================
 */

#include <gmodule.h>

#include "hev-scgi-handler.h"
#include "hev-scgi-task.h"
#include "hev-scgi-request.h"
#include "hev-scgi-response.h"

#define HEV_SCGI_HANDLER_CGI_NAME		"HevSCGIHandlerCGI"
#define HEV_SCGI_HANDLER_CGI_VERSION	"0.0.4"
#define HEV_SCGI_HANDLER_CGI_PATTERN	"^.*\\.php($|\\?).*"

#define HEV_SCGI_HANDLER_CGI_BIN_PATH	"/usr/bin/php-cgi"
#define HEV_SCGI_HANDLER_CGI_WORK_DIR	"/tmp"

#define BUFFER_SIZE                     4096

typedef struct _HevSCGIHandlerCGITaskData HevSCGIHandlerCGITaskData;

struct _HevSCGIHandlerCGITaskData
{
	GObject *scgi_request;
	GObject *scgi_response;

	GInputStream *req_input_stream;
	GOutputStream *res_output_stream;
	GInputStream *child_input_stream;
	GOutputStream *child_output_stream;

	GHashTable *req_hash_table;

	gchar **envp;
	guint envi;
	gchar in_buffer[BUFFER_SIZE];
	gchar out_buffer[BUFFER_SIZE];
	guint in_count;
	guint out_count;
	guint content_len;
	gboolean is_checked;
};

static void req_hash_table_foreach_handler(gpointer key,
			gpointer value, gpointer user_data);
static void req_input_stream_read_async_handler(GObject *source_object,
			GAsyncResult *res, gpointer user_data);
static void res_output_stream_write_async_handler(GObject *source_object,
			GAsyncResult *res, gpointer user_data);
static void child_input_stream_read_async_handler(GObject *source_object,
			GAsyncResult *res, gpointer user_data);
static void child_output_stream_write_async_handler(GObject *source_object,
			GAsyncResult *res, gpointer user_data);
static void child_output_stream_close_async_handler(GObject *source_object,
			GAsyncResult *res, gpointer user_data);
static void child_input_stream_close_async_handler(GObject *source_object,
			GAsyncResult *res, gpointer user_data);
static void hev_scgi_handler_cgi_task_data_destroy_handler(gpointer data);

G_MODULE_EXPORT gboolean hev_scgi_handler_module_init(HevSCGIHandler *self)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);
	return TRUE;
}

G_MODULE_EXPORT void hev_scgi_handler_module_finalize(HevSCGIHandler *self)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);
}

G_MODULE_EXPORT const gchar * hev_scgi_handler_module_get_name(HevSCGIHandler *self)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);
	return HEV_SCGI_HANDLER_CGI_NAME;
}

G_MODULE_EXPORT const gchar * hev_scgi_handler_module_get_version(HevSCGIHandler *self)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);
	return HEV_SCGI_HANDLER_CGI_VERSION;
}

G_MODULE_EXPORT const gchar * hev_scgi_handler_module_get_pattern(HevSCGIHandler *self)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);
	return HEV_SCGI_HANDLER_CGI_PATTERN;
}

G_MODULE_EXPORT void hev_scgi_handler_module_handle(HevSCGIHandler *self, GObject *scgi_task)
{
	HevSCGIHandlerCGITaskData *task_data = NULL;
	gchar *argv[] =
	{
		HEV_SCGI_HANDLER_CGI_BIN_PATH,
		NULL
	};
	gint in = 0, out = 0;
	GPid pid = 0;
	GError *error = NULL;

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	task_data = g_slice_new0(HevSCGIHandlerCGITaskData);
	if(!task_data)
	{
		g_critical("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);
		return;
	}
	g_object_set_data_full(scgi_task, "data", task_data,
				hev_scgi_handler_cgi_task_data_destroy_handler);

	task_data->scgi_request = hev_scgi_task_get_request(HEV_SCGI_TASK(scgi_task));
	task_data->scgi_response = hev_scgi_task_get_response(HEV_SCGI_TASK(scgi_task));
	task_data->req_hash_table =
		hev_scgi_request_get_header_hash_table(HEV_SCGI_REQUEST(task_data->scgi_request));
	task_data->req_input_stream =
		hev_scgi_request_get_input_stream(HEV_SCGI_REQUEST(task_data->scgi_request));
	task_data->res_output_stream =
		hev_scgi_response_get_output_stream(HEV_SCGI_RESPONSE(task_data->scgi_response));
	task_data->content_len = atoi(g_hash_table_lookup(task_data->req_hash_table, "CONTENT_LENGTH"));

	task_data->envp =
		g_malloc0_n(g_hash_table_size(task_data->req_hash_table)+1, sizeof(gchar *));
	g_hash_table_foreach(task_data->req_hash_table, req_hash_table_foreach_handler, scgi_task);

	if(g_spawn_async_with_pipes(HEV_SCGI_HANDLER_CGI_WORK_DIR, argv, task_data->envp,
					G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL,
					&pid, &in, &out, NULL, &error))
	{
		task_data->child_input_stream = g_unix_input_stream_new(out, TRUE);
		task_data->child_output_stream = g_unix_output_stream_new(in, TRUE);
		task_data->is_checked = FALSE;

		g_input_stream_read_async(task_data->req_input_stream, task_data->in_buffer,
					(task_data->content_len<BUFFER_SIZE)?task_data->content_len:BUFFER_SIZE,
					G_PRIORITY_DEFAULT, NULL, req_input_stream_read_async_handler,
					scgi_task);
		g_object_ref(scgi_task);
		g_input_stream_read_async(task_data->child_input_stream, task_data->out_buffer,
					BUFFER_SIZE, G_PRIORITY_DEFAULT, NULL,
					child_input_stream_read_async_handler, scgi_task);
		g_object_ref(scgi_task);
	}
	else
	{
		g_critical("%s:%d[%s]=>(%s)", __FILE__, __LINE__,
					__FUNCTION__, error->message);
		g_error_free(error);
	}
}

static void req_hash_table_foreach_handler(gpointer key,
			gpointer value, gpointer user_data)
{
	GObject *scgi_task = user_data;
	HevSCGIHandlerCGITaskData *task_data = NULL;

	task_data = g_object_get_data(scgi_task, "data");
	task_data->envp[task_data->envi++] = g_strdup_printf("%s=%s",
				key, value);
}

static void req_input_stream_read_async_handler(GObject *source_object,
			GAsyncResult *res, gpointer user_data)
{
	GObject *scgi_task = user_data;
	HevSCGIHandlerCGITaskData *task_data = NULL;
	gssize size = 0;

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	task_data = g_object_get_data(scgi_task, "data");

	size = g_input_stream_read_finish(G_INPUT_STREAM(source_object),
				res, NULL);
	if(0 <= size)
	{
		g_output_stream_write_async(task_data->child_output_stream,
					task_data->in_buffer, size, G_PRIORITY_DEFAULT, NULL,
					child_output_stream_write_async_handler, scgi_task);
	}
	else
	{
		g_object_unref(scgi_task);
	}
}

static void res_output_stream_write_async_handler(GObject *source_object,
			GAsyncResult *res, gpointer user_data)
{
	GObject *scgi_task = user_data;
	HevSCGIHandlerCGITaskData *task_data = NULL;
	gssize size = 0;

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	task_data = g_object_get_data(scgi_task, "data");

	size = g_output_stream_write_finish(G_OUTPUT_STREAM(source_object),
				res, NULL);
	if(0 < size)
	{
		if(FALSE == task_data->is_checked)
		{
			g_output_stream_write_async(G_OUTPUT_STREAM(source_object),
						task_data->out_buffer, task_data->out_count,
						G_PRIORITY_DEFAULT, NULL,
						res_output_stream_write_async_handler, scgi_task);
			task_data->is_checked = TRUE;
		}
		else
		{
			task_data->out_count = 0;
			g_input_stream_read_async(task_data->child_input_stream,
						task_data->out_buffer, BUFFER_SIZE,
						G_PRIORITY_DEFAULT, NULL,
						child_input_stream_read_async_handler, scgi_task);
		}
	}
	else if(0 >= size)
	{
		g_input_stream_close_async(task_data->child_input_stream,
					G_PRIORITY_DEFAULT, NULL, 
					child_input_stream_close_async_handler,
					scgi_task);

		g_object_unref(scgi_task);
	}
}

static void child_input_stream_read_async_handler(GObject *source_object,
			GAsyncResult *res, gpointer user_data)
{
	GObject *scgi_task = user_data;
	HevSCGIHandlerCGITaskData *task_data = NULL;
	gssize size = 0;

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	task_data = g_object_get_data(scgi_task, "data");

	size = g_input_stream_read_finish(G_INPUT_STREAM(source_object),
				res, NULL);
	if(0 <= size)
	{
		task_data->out_count += size;
		if((0==size) || (BUFFER_SIZE<=task_data->out_count))
		{
			gchar *header_end = NULL;

			if((FALSE==task_data->is_checked) &&
					(header_end=g_strstr_len(task_data->out_buffer,
											 task_data->out_count, "\r\n\r\n")) &&
					(!g_strstr_len(task_data->out_buffer,
								   header_end-task_data->out_buffer, "Status:")))
			{
				g_output_stream_write_async(task_data->res_output_stream,
							"Status: 200 OK\r\n", 16, G_PRIORITY_DEFAULT, NULL,
							res_output_stream_write_async_handler, scgi_task);
			}
			else
			{
				task_data->is_checked = TRUE;
				g_output_stream_write_async(task_data->res_output_stream,
							task_data->out_buffer, task_data->out_count,
							G_PRIORITY_DEFAULT, NULL,
							res_output_stream_write_async_handler, scgi_task);
			}
		}
		else
		{
			g_input_stream_read_async(G_INPUT_STREAM(source_object),
						task_data->out_buffer+task_data->out_count,
						BUFFER_SIZE-task_data->out_count, G_PRIORITY_DEFAULT, NULL,
						child_input_stream_read_async_handler, scgi_task);
		}

	}
	else
	{
		g_object_unref(scgi_task);
	}
}

static void child_output_stream_write_async_handler(GObject *source_object,
			GAsyncResult *res, gpointer user_data)
{
	GObject *scgi_task = user_data;
	HevSCGIHandlerCGITaskData *task_data = NULL;
	gssize size = 0;

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	task_data = g_object_get_data(scgi_task, "data");

	size = g_output_stream_write_finish(G_OUTPUT_STREAM(source_object),
				res, NULL);
	if(0 < size)
	{
		guint len = 0;

		task_data->in_count += size;
		len = task_data->content_len - task_data->in_count;
		g_input_stream_read_async(task_data->req_input_stream, task_data->in_buffer,
					(len<BUFFER_SIZE)?len:BUFFER_SIZE, G_PRIORITY_DEFAULT, NULL,
					req_input_stream_read_async_handler,
					scgi_task);
	}
	else if(0 >= size)
	{
		g_output_stream_close_async(G_OUTPUT_STREAM(source_object),
					G_PRIORITY_DEFAULT, NULL, 
					child_output_stream_close_async_handler,
					scgi_task);

		g_object_unref(scgi_task);
	}
}

static void child_output_stream_close_async_handler(GObject *source_object,
			GAsyncResult *res, gpointer user_data)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	g_output_stream_close_finish(G_OUTPUT_STREAM(source_object),
				res, NULL);
}

static void child_input_stream_close_async_handler(GObject *source_object,
			GAsyncResult *res, gpointer user_data)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	g_input_stream_close_finish(G_INPUT_STREAM(source_object),
				res, NULL);
}

static void hev_scgi_handler_cgi_task_data_destroy_handler(gpointer data)
{
	HevSCGIHandlerCGITaskData *task_data = data;

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	g_strfreev(task_data->envp);

	g_slice_free(HevSCGIHandlerCGITaskData, task_data);
}

