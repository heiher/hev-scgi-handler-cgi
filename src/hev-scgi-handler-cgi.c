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
#define HEV_SCGI_HANDLER_CGI_VERSION	"0.0.3"
#define HEV_SCGI_HANDLER_CGI_PATTERN	"^.*\\.php($|\\?).*"

#define HEV_SCGI_HANDLER_CGI_BIN_PATH	"/usr/bin/php-cgi"
#define HEV_SCGI_HANDLER_CGI_WORK_DIR	"/tmp"

#define BUFFER_SIZE                     4096

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
	GObject *scgi_request = NULL;
	GObject *scgi_response = NULL;
	GHashTable *req_hash_table = NULL;
	GInputStream *input_stream = NULL;
	gint content_length = 0;
	gchar *argv[] =
	{
		HEV_SCGI_HANDLER_CGI_BIN_PATH,
		NULL
	};
	gchar **envp = NULL;
	gint *envip = NULL;
	gint in = 0, out = 0;
	GPid pid = 0;
	GError *error = NULL;

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	scgi_request = hev_scgi_task_get_request(HEV_SCGI_TASK(scgi_task));
	scgi_response = hev_scgi_task_get_response(HEV_SCGI_TASK(scgi_task));

	req_hash_table = hev_scgi_request_get_header_hash_table(HEV_SCGI_REQUEST(scgi_request));

	input_stream = hev_scgi_request_get_input_stream(HEV_SCGI_REQUEST(scgi_request));

	content_length = atoi(g_hash_table_lookup(req_hash_table, "CONTENT_LENGTH"));

	envp = g_malloc0_n(g_hash_table_size(req_hash_table)+1, sizeof(gchar *));
	envip = g_malloc0(sizeof(gint));
	g_object_set_data_full(scgi_task, "envp", envp,
				(GDestroyNotify)g_strfreev);
	g_object_set_data_full(scgi_task, "envip", envip, g_free);
	g_hash_table_foreach(req_hash_table, req_hash_table_foreach_handler, scgi_task);

	if(g_spawn_async_with_pipes(HEV_SCGI_HANDLER_CGI_WORK_DIR, argv, envp,
					G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL,
					&pid, &in, &out, NULL, &error))
	{
		GInputStream *child_input_stream = NULL;
		GOutputStream *child_output_stream = NULL;
		gchar *in_buffer = NULL;
		gchar *out_buffer = NULL;
		guint *in_count = NULL;
		guint *out_count = NULL;
		guint *content_len = NULL;
		gboolean *is_checked = NULL;

		child_input_stream = g_unix_input_stream_new(out, TRUE);
		child_output_stream = g_unix_output_stream_new(in, TRUE);
		g_object_set_data(scgi_task, "child_input_stream",
					child_input_stream);
		g_object_set_data(scgi_task, "child_output_stream",
					child_output_stream);

		in_buffer = g_malloc0(BUFFER_SIZE);
		g_object_set_data_full(scgi_task, "in_buffer",
					in_buffer, g_free);
		out_buffer = g_malloc0(BUFFER_SIZE);
		g_object_set_data_full(scgi_task, "out_buffer",
					out_buffer, g_free);
		in_count = g_malloc0(sizeof(guint));
		g_object_set_data_full(scgi_task, "in_count",
					in_count, g_free);
		out_count = g_malloc0(sizeof(guint));
		g_object_set_data_full(scgi_task, "out_count",
					out_count, g_free);
		content_len = g_malloc0(sizeof(guint));
		g_object_set_data_full(scgi_task, "content_len",
					content_len, g_free);
		is_checked = g_malloc0(sizeof(gboolean));
		g_object_set_data_full(scgi_task, "is_checked",
					is_checked, g_free);
		*is_checked = FALSE;

		*content_len = content_length;

		g_input_stream_read_async(input_stream, in_buffer,
					(content_length<BUFFER_SIZE)?content_length:BUFFER_SIZE,
					G_PRIORITY_DEFAULT, NULL, req_input_stream_read_async_handler,
					scgi_task);
		g_object_ref(scgi_task);
		g_input_stream_read_async(child_input_stream, out_buffer,
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
	gchar **envp = g_object_get_data(scgi_task, "envp");
	gint *envi = g_object_get_data(scgi_task, "envip");

	envp[(*envi)++] = g_strdup_printf("%s=%s",
				key, value);
}

static void req_input_stream_read_async_handler(GObject *source_object,
			GAsyncResult *res, gpointer user_data)
{
	GObject *scgi_task = user_data;
	gssize size = 0;

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	size = g_input_stream_read_finish(G_INPUT_STREAM(source_object),
				res, NULL);
	if(0 <= size)
	{
		GOutputStream *child_output_stream = NULL;
		guint *in_buffer = g_object_get_data(scgi_task,
					"in_buffer");
		
		child_output_stream = g_object_get_data(scgi_task,
					"child_output_stream");

		g_output_stream_write_async(child_output_stream,
					in_buffer, size, G_PRIORITY_DEFAULT, NULL,
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
	gssize size = 0;
	GInputStream *child_input_stream = NULL;

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	size = g_output_stream_write_finish(G_OUTPUT_STREAM(source_object),
				res, NULL);

	child_input_stream = g_object_get_data(scgi_task,
				"child_input_stream");

	if(0 < size)
	{
		gchar *out_buffer = NULL;
		guint *out_count = NULL;
		gboolean *is_checked = NULL;

		out_buffer = g_object_get_data(scgi_task, "out_buffer");
		out_count = g_object_get_data(scgi_task, "out_count");
		is_checked = g_object_get_data(scgi_task, "is_checked");

		if(FALSE == (*is_checked))
		{
			g_output_stream_write_async(G_OUTPUT_STREAM(source_object),
						out_buffer, *out_count, G_PRIORITY_DEFAULT, NULL,
						res_output_stream_write_async_handler, scgi_task);
			*is_checked = TRUE;
		}
		else
		{
			*out_count = 0;
			g_input_stream_read_async(child_input_stream, out_buffer,
						BUFFER_SIZE, G_PRIORITY_DEFAULT, NULL,
						child_input_stream_read_async_handler, scgi_task);
		}
	}
	else if(0 >= size)
	{
		g_input_stream_close_async(child_input_stream,
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
	gssize size = 0;

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	size = g_input_stream_read_finish(G_INPUT_STREAM(source_object),
				res, NULL);
	if(0 <= size)
	{
		gchar *out_buffer = NULL;
		guint *out_count = NULL;

		out_buffer = g_object_get_data(scgi_task, "out_buffer");
		out_count = g_object_get_data(scgi_task, "out_count");

		*out_count += size;
		if((0==size) || (BUFFER_SIZE<=(*out_count)))
		{
			GObject *scgi_response = NULL;
			GOutputStream *output_stream = NULL;
			gchar *header_end = NULL;
			gboolean *is_checked = NULL;

			scgi_response = hev_scgi_task_get_response(HEV_SCGI_TASK(scgi_task));
			output_stream = hev_scgi_response_get_output_stream(HEV_SCGI_RESPONSE(scgi_response));
			is_checked = g_object_get_data(scgi_task, "is_checked");

			if((FALSE==(*is_checked)) &&
						(header_end=g_strstr_len(out_buffer,
												 *out_count, "\r\n\r\n")) &&
						(NULL==g_strstr_len(out_buffer, header_end-out_buffer,
									  "Status:")))
			{
				g_output_stream_write_async(output_stream,
							"Status: 200 OK\r\n", 16, G_PRIORITY_DEFAULT, NULL,
							res_output_stream_write_async_handler, scgi_task);
			}
			else
			{
				*is_checked = TRUE;
				g_output_stream_write_async(output_stream,
							out_buffer, *out_count, G_PRIORITY_DEFAULT, NULL,
							res_output_stream_write_async_handler, scgi_task);
			}
		}
		else
		{
			g_input_stream_read_async(G_INPUT_STREAM(source_object),
						out_buffer+(*out_count), BUFFER_SIZE-(*out_count),
						G_PRIORITY_DEFAULT, NULL,
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
	gssize size = 0;

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	size = g_output_stream_write_finish(G_OUTPUT_STREAM(source_object),
				res, NULL);

	if(0 < size)
	{
		GObject *scgi_request = NULL;
		GInputStream *input_stream = NULL;
		guint *in_buffer = g_object_get_data(scgi_task, "in_buffer");
		guint *in_count = g_object_get_data(scgi_task, "in_count");
		guint *content_len = g_object_get_data(scgi_task, "content_len");
		guint len = 0;

		scgi_request = hev_scgi_task_get_request(HEV_SCGI_TASK(scgi_task));
		input_stream = hev_scgi_request_get_input_stream(HEV_SCGI_REQUEST(scgi_request));

		*in_count += size;
		len = *content_len - *in_count;
		g_input_stream_read_async(input_stream, in_buffer,
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

