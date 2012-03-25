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
#define HEV_SCGI_HANDLER_CGI_VERSION	"0.0.5"
#define HEV_SCGI_HANDLER_CGI_PATTERN	".*"

#define HEV_SCGI_HANDLER_CGI_BIN_PATH	"/usr/bin/php-cgi"
#define HEV_SCGI_HANDLER_CGI_WORK_DIR	"/tmp"

typedef struct _HevSCGIHandlerCGITaskData HevSCGIHandlerCGITaskData;

struct _HevSCGIHandlerCGITaskData
{
	GObject *scgi_request;
	GObject *scgi_response;
	GHashTable *req_hash_table;

	gchar **envp;
	guint envi;
	gint fd;
};

static void req_hash_table_foreach_handler(gpointer key,
			gpointer value, gpointer user_data);
static void hev_scgi_handler_cgi_task_data_destroy_handler(gpointer data);
static void hev_scgi_handler_spawn_child_setup_handler(gpointer user_data);
static void hev_scgi_handler_child_watch_handler(GPid pid, gint status,
			gpointer user_data);

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
	gchar *str = NULL, **argv = NULL, *workdir = NULL;
	GPid pid = 0;
	GError *error = NULL;
	GSocketConnection *connection = NULL;
	GSocket *socket = NULL;

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	task_data = g_slice_new0(HevSCGIHandlerCGITaskData);
	if(!task_data)
	{
		g_critical("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);
		return;
	}
	g_object_set_data_full(scgi_task, "data", task_data,
				hev_scgi_handler_cgi_task_data_destroy_handler);

	connection = hev_scgi_task_get_socket_connection(HEV_SCGI_TASK(scgi_task));
	socket = g_socket_connection_get_socket(G_SOCKET_CONNECTION(connection));
	task_data->fd = g_socket_get_fd(socket);

	task_data->scgi_request = hev_scgi_task_get_request(HEV_SCGI_TASK(scgi_task));
	task_data->scgi_response = hev_scgi_task_get_response(HEV_SCGI_TASK(scgi_task));
	task_data->req_hash_table =
		hev_scgi_request_get_header_hash_table(HEV_SCGI_REQUEST(task_data->scgi_request));

	task_data->envp =
		g_malloc0_n(g_hash_table_size(task_data->req_hash_table)+1, sizeof(gchar *));
	g_hash_table_foreach(task_data->req_hash_table, req_hash_table_foreach_handler, scgi_task);

	str = g_hash_table_lookup(task_data->req_hash_table, "SCRIPT_FILENAME");
	argv = g_malloc0_n(2, sizeof(gchar *));
	if(str)
	{
		argv[0] = g_strdup(str);
		workdir = g_path_get_dirname(str);
	}
	else
	{
		argv[0] = g_strdup(HEV_SCGI_HANDLER_CGI_BIN_PATH);
		workdir = g_strdup(HEV_SCGI_HANDLER_CGI_WORK_DIR);
	}

	if(g_spawn_async(workdir, argv, task_data->envp, G_SPAWN_DO_NOT_REAP_CHILD,
					hev_scgi_handler_spawn_child_setup_handler,
					task_data, &pid, &error))
	{
		g_object_ref(scgi_task);
		g_child_watch_add(pid, hev_scgi_handler_child_watch_handler, scgi_task);
	}
	else
	{
		g_critical("%s:%d[%s]=>(%s)", __FILE__, __LINE__,
					__FUNCTION__, error->message);
		g_error_free(error);
	}

	g_strfreev(argv);
	g_free(workdir);
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

static void hev_scgi_handler_cgi_task_data_destroy_handler(gpointer data)
{
	HevSCGIHandlerCGITaskData *task_data = data;

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	g_strfreev(task_data->envp);
	g_slice_free(HevSCGIHandlerCGITaskData, task_data);
}

static void hev_scgi_handler_spawn_child_setup_handler(gpointer user_data)
{
	HevSCGIHandlerCGITaskData *task_data = user_data;

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	g_unix_set_fd_nonblocking(task_data->fd, FALSE, NULL);

	dup2(task_data->fd, 0);
	dup2(task_data->fd, 1);
}

static void hev_scgi_handler_child_watch_handler(GPid pid, gint status,
			gpointer user_data)
{
	GObject *scgi_task = G_OBJECT(user_data);
	HevSCGIHandlerCGITaskData *task_data = NULL;

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	task_data = g_object_get_data(scgi_task, "data");
	g_unix_set_fd_nonblocking(task_data->fd, TRUE, NULL);
	g_spawn_close_pid(pid);
	g_object_unref(scgi_task);
}

