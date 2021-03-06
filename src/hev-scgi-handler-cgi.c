/*
 ============================================================================
 Name        : hev-scgi-handler-cgi.c
 Author      : Heiher <root@heiher.info>
 Version     : 0.0.8
 Copyright   : Copyright (C) 2013 everyone.
 Description : 
 ============================================================================
 */

#include <stdlib.h>
#include <string.h>
#include <hev-scgi-1.0.h>
#include <glib-unix.h>

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

#include "hev-scgi-handler-cgi.h"

#define HEV_SCGI_HANDLER_CGI_NAME		"HevSCGIHandlerCGI"
#define HEV_SCGI_HANDLER_CGI_VERSION	"1.0.0"

#define HEV_SCGI_HANDLER_CGI_BIN_PATH	"/usr/bin/php-cgi"
#define HEV_SCGI_HANDLER_CGI_WORK_DIR	"/tmp"

enum
{
	PROP_0,
	PROP_CONFIG,
	N_PROPERTIES
};

static GParamSpec *hev_scgi_handler_cgi_properties[N_PROPERTIES] = { NULL };

#define HEV_SCGI_HANDLER_CGI_GET_PRIVATE(obj)	(G_TYPE_INSTANCE_GET_PRIVATE((obj), HEV_TYPE_SCGI_HANDLER_CGI, HevSCGIHandlerCGIPrivate))

typedef struct _HevSCGIHandlerCGIPrivate HevSCGIHandlerCGIPrivate;

struct _HevSCGIHandlerCGIPrivate
{
	GKeyFile *config;
	gchar *alias;
	gchar *pattern;
};

typedef struct _HevSCGIHandlerCGITaskData HevSCGIHandlerCGITaskData;

struct _HevSCGIHandlerCGITaskData
{
	GObject *scgi_task;
	GObject *scgi_request;
	GObject *scgi_response;
	GHashTable *req_hash_table;

	gchar *user;
	gchar *group;

	gchar **envp;
	guint envi;
	gint fd;
};

static void hev_scgi_handler_iface_init(HevSCGIHandlerInterface * iface);

#ifdef STATIC_MODULE
G_DEFINE_TYPE_EXTENDED(HevSCGIHandlerCGI, hev_scgi_handler_cgi, G_TYPE_OBJECT, 0,
			G_IMPLEMENT_INTERFACE(HEV_TYPE_SCGI_HANDLER, hev_scgi_handler_iface_init));
#else /* STATIC_MODULE */
G_DEFINE_DYNAMIC_TYPE_EXTENDED(HevSCGIHandlerCGI, hev_scgi_handler_cgi, G_TYPE_OBJECT, 0,
			G_IMPLEMENT_INTERFACE_DYNAMIC(HEV_TYPE_SCGI_HANDLER, hev_scgi_handler_iface_init));

void hev_scgi_handler_cgi_reg_type(GTypeModule *module)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	if(G_TYPE_INVALID == HEV_TYPE_SCGI_HANDLER_CGI)
	  hev_scgi_handler_cgi_register_type(module);
}
#endif /* !STATIC_MODULE */

static const gchar * hev_scgi_handler_cgi_get_alias(HevSCGIHandler *handler);
static const gchar * hev_scgi_handler_cgi_get_name(HevSCGIHandler *handler);
static const gchar * hev_scgi_handler_cgi_get_version(HevSCGIHandler *handler);
static const gchar * hev_scgi_handler_cgi_get_pattern(HevSCGIHandler *handler);
static void hev_scgi_handler_cgi_handle(HevSCGIHandler *self, GObject *scgi_task);

static void req_hash_table_foreach_handler(gpointer key,
			gpointer value, gpointer user_data);
static void hev_scgi_handler_cgi_task_data_free(HevSCGIHandlerCGITaskData *task_data);
static void hev_scgi_handler_spawn_child_setup_handler(gpointer user_data);
static void hev_scgi_handler_child_watch_handler(GPid pid, gint status,
			gpointer user_data);
static void hev_scgi_handler_response_error_message(HevSCGIHandlerCGITaskData *task_data,
			const gchar *message);

static void hev_scgi_handler_cgi_dispose(GObject *obj)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	G_OBJECT_CLASS(hev_scgi_handler_cgi_parent_class)->dispose(obj);
}

static void hev_scgi_handler_cgi_finalize(GObject *obj)
{
	HevSCGIHandlerCGI *self = HEV_SCGI_HANDLER_CGI(obj);
	HevSCGIHandlerCGIPrivate *priv = HEV_SCGI_HANDLER_CGI_GET_PRIVATE(self);

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	if(priv->config)
	{
		g_key_file_free(priv->config);
		priv->config = NULL;
	}

	if(priv->alias)
	{
		g_free(priv->alias);
		priv->alias = NULL;
	}

	if(priv->pattern)
	{
		g_free(priv->pattern);
		priv->pattern = NULL;
	}

	G_OBJECT_CLASS(hev_scgi_handler_cgi_parent_class)->finalize(obj);
}

static void hev_scgi_handler_cgi_class_finalize(HevSCGIHandlerCGIClass *klass)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);
}

static GObject * hev_scgi_handler_cgi_constructor(GType type, guint n, GObjectConstructParam *param)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	return G_OBJECT_CLASS(hev_scgi_handler_cgi_parent_class)->constructor(type, n, param);
}

static void hev_scgi_handler_cgi_constructed(GObject *obj)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	G_OBJECT_CLASS(hev_scgi_handler_cgi_parent_class)->constructed(obj);
}

static void hev_scgi_handler_cgi_set_property(GObject *obj,
			guint prop_id, const GValue *value, GParamSpec *pspec)
{
	HevSCGIHandlerCGI *self = HEV_SCGI_HANDLER_CGI(obj);
	HevSCGIHandlerCGIPrivate *priv = HEV_SCGI_HANDLER_CGI_GET_PRIVATE(self);

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	switch(prop_id)
	{
	case PROP_CONFIG:
			priv->config = g_value_get_pointer(value);
			break;
	default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
			break;
	}
}

static void hev_scgi_handler_cgi_get_property(GObject *obj,
			guint prop_id, GValue *value, GParamSpec *pspec)
{
	HevSCGIHandlerCGI *self = HEV_SCGI_HANDLER_CGI(obj);
	HevSCGIHandlerCGIPrivate *priv = HEV_SCGI_HANDLER_CGI_GET_PRIVATE(self);

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	switch(prop_id)
	{
	case PROP_CONFIG:
			g_value_set_pointer(value, priv->config);
			break;
	default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
			break;
	}
}

static void hev_scgi_handler_cgi_class_init(HevSCGIHandlerCGIClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	obj_class->constructor = hev_scgi_handler_cgi_constructor;
	obj_class->constructed = hev_scgi_handler_cgi_constructed;
	obj_class->dispose = hev_scgi_handler_cgi_dispose;
	obj_class->finalize = hev_scgi_handler_cgi_finalize;

	obj_class->set_property = hev_scgi_handler_cgi_set_property;
	obj_class->get_property = hev_scgi_handler_cgi_get_property;

	hev_scgi_handler_cgi_properties[PROP_CONFIG] =
		g_param_spec_pointer ("config", "Config", "The module config",
					G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
	g_object_class_install_properties(obj_class, N_PROPERTIES,
				hev_scgi_handler_cgi_properties);

	g_type_class_add_private(klass, sizeof(HevSCGIHandlerCGIPrivate));
}

static void hev_scgi_handler_iface_init(HevSCGIHandlerInterface * iface)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	iface->get_alias = hev_scgi_handler_cgi_get_alias;
	iface->get_name = hev_scgi_handler_cgi_get_name;
	iface->get_version = hev_scgi_handler_cgi_get_version;
	iface->get_pattern = hev_scgi_handler_cgi_get_pattern;
	iface->handle = hev_scgi_handler_cgi_handle;
}

static void hev_scgi_handler_cgi_init(HevSCGIHandlerCGI *self)
{
	HevSCGIHandlerCGIPrivate *priv = HEV_SCGI_HANDLER_CGI_GET_PRIVATE(self);

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	priv->config = NULL;
}

static const gchar * hev_scgi_handler_cgi_get_alias(HevSCGIHandler *handler)
{
	HevSCGIHandlerCGI *self = HEV_SCGI_HANDLER_CGI(handler);
	HevSCGIHandlerCGIPrivate *priv = HEV_SCGI_HANDLER_CGI_GET_PRIVATE(self);

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	if(!priv->alias)
	  priv->alias = g_key_file_get_string(priv->config,
				  "Module", "Alias", NULL);

	return priv->alias;
}

static const gchar * hev_scgi_handler_cgi_get_name(HevSCGIHandler *handler)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	return HEV_SCGI_HANDLER_CGI_NAME;
}

static const gchar * hev_scgi_handler_cgi_get_version(HevSCGIHandler *handler)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	return HEV_SCGI_HANDLER_CGI_VERSION;
}

static const gchar * hev_scgi_handler_cgi_get_pattern(HevSCGIHandler *handler)
{
	HevSCGIHandlerCGI *self = HEV_SCGI_HANDLER_CGI(handler);
	HevSCGIHandlerCGIPrivate *priv = HEV_SCGI_HANDLER_CGI_GET_PRIVATE(self);

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	if(!priv->pattern)
	  priv->pattern = g_key_file_get_string(priv->config,
				  "Module", "Pattern", NULL);

	return priv->pattern;
}

static void hev_scgi_handler_cgi_handle(HevSCGIHandler *handler, GObject *scgi_task)
{
	HevSCGIHandlerCGI *self = HEV_SCGI_HANDLER_CGI(handler);
	HevSCGIHandlerCGIPrivate *priv = HEV_SCGI_HANDLER_CGI_GET_PRIVATE(self);

	HevSCGIHandlerCGITaskData *task_data = NULL;
	gchar *str = NULL, **argv = NULL, *workdir = NULL;
	GPid pid = 0;
	GError *error = NULL;
	GObject *connection = NULL;
	GSocket *socket = NULL;

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	task_data = g_slice_new0(HevSCGIHandlerCGITaskData);
	if(!task_data)
	{
		g_critical("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);
		return;
	}

	connection = hev_scgi_task_get_socket_connection(HEV_SCGI_TASK(scgi_task));
	socket = g_socket_connection_get_socket(G_SOCKET_CONNECTION(connection));
	task_data->fd = g_socket_get_fd(socket);

	task_data->scgi_task = scgi_task;
	task_data->scgi_request = hev_scgi_task_get_request(HEV_SCGI_TASK(scgi_task));
	task_data->scgi_response = hev_scgi_task_get_response(HEV_SCGI_TASK(scgi_task));
	task_data->req_hash_table =
		hev_scgi_request_get_header_hash_table(HEV_SCGI_REQUEST(task_data->scgi_request));

	task_data->envp =
		g_malloc0_n(g_hash_table_size(task_data->req_hash_table)+1, sizeof(gchar *));
	g_hash_table_foreach(task_data->req_hash_table, req_hash_table_foreach_handler, task_data);

	/* Script file and Work dir */
	str = g_hash_table_lookup(task_data->req_hash_table, "SCRIPT_FILE");
	argv = g_malloc0_n(2, sizeof(gchar *));
	if(str)
	{
		argv[0] = g_strdup(str);
		workdir = g_path_get_dirname(str);
	}
	else
	{
		argv[0] = g_key_file_get_string(priv->config, "Module", "CGIBinPath", NULL);
		workdir = g_key_file_get_string(priv->config, "Module", "WorkDir", NULL);

		if(NULL == argv[0])
		  argv[0] = g_strdup(HEV_SCGI_HANDLER_CGI_BIN_PATH);
		if(NULL == workdir)
		  workdir = g_strdup(HEV_SCGI_HANDLER_CGI_WORK_DIR);
	}

	/* User and Group */
	str = g_hash_table_lookup(task_data->req_hash_table, "_USER");
	if(str)
	  task_data->user = g_strdup(str);
	else
	  task_data->user = g_key_file_get_string(priv->config, "Module", "User", NULL);

	str = g_hash_table_lookup(task_data->req_hash_table, "_GROUP");
	if(str)
	  task_data->group = g_strdup(str);
	else
	  task_data->group = g_key_file_get_string(priv->config, "Module", "Group", NULL);

	if(g_spawn_async(workdir, argv, task_data->envp, G_SPAWN_DO_NOT_REAP_CHILD,
					hev_scgi_handler_spawn_child_setup_handler,
					task_data, &pid, &error))
	{
		g_object_ref(scgi_task);
		g_child_watch_add(pid, hev_scgi_handler_child_watch_handler, task_data);
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
	HevSCGIHandlerCGITaskData *task_data = user_data;

	task_data->envp[task_data->envi++] = g_strdup_printf("%s=%s",
				(char *) key, (char *) value);
}

static void hev_scgi_handler_cgi_task_data_free(HevSCGIHandlerCGITaskData *task_data)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	g_free(task_data->user);
	g_free(task_data->group);
	g_strfreev(task_data->envp);
	g_slice_free(HevSCGIHandlerCGITaskData, task_data);
}

static void hev_scgi_handler_spawn_child_setup_handler(gpointer user_data)
{
	HevSCGIHandlerCGITaskData *task_data = user_data;

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	if(task_data->group)
	{
		struct group *grp = NULL;

		if(NULL == (grp = getgrnam(task_data->group)))
		  hev_scgi_handler_response_error_message(task_data, "Get group failed!");

		if(-1 == setgid(grp->gr_gid))
		  hev_scgi_handler_response_error_message(task_data, "Set gid failed!");
		if(-1 == setgroups(0, NULL))
		  hev_scgi_handler_response_error_message(task_data, "Set groups failed!");
		if(task_data->user)
		{
			if(-1 == initgroups(task_data->user, grp->gr_gid))
			  hev_scgi_handler_response_error_message(task_data, "Init groups failed!");
		}
	}

	if(task_data->user)
	{
		struct passwd *pwd = NULL;

		if(NULL == (pwd = getpwnam(task_data->user)))
		  hev_scgi_handler_response_error_message(task_data, "Get user failed!");

		if(-1 == setuid(pwd->pw_uid))
		  hev_scgi_handler_response_error_message(task_data, "Set uid failed!");
	}

	g_unix_set_fd_nonblocking(task_data->fd, FALSE, NULL);

	dup2(task_data->fd, STDIN_FILENO);
	dup2(task_data->fd, STDOUT_FILENO);
}

static void hev_scgi_handler_child_watch_handler(GPid pid, gint status,
			gpointer user_data)
{
	HevSCGIHandlerCGITaskData *task_data = user_data;

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	g_unix_set_fd_nonblocking(task_data->fd, TRUE, NULL);
	g_spawn_close_pid(pid);
	g_object_unref(task_data->scgi_task);
	hev_scgi_handler_cgi_task_data_free(task_data);
}

static void hev_scgi_handler_response_error_message(HevSCGIHandlerCGITaskData *task_data,
			const gchar *message)
{
	GHashTable *res_hash_table = NULL;
	GOutputStream *output_stream = NULL;

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	res_hash_table =
		hev_scgi_response_get_header_hash_table(HEV_SCGI_RESPONSE(task_data->scgi_response));
	g_hash_table_insert(res_hash_table, g_strdup("Status"), g_strdup("500 Internal Server Error"));
	g_hash_table_insert(res_hash_table, g_strdup("ContentType"), g_strdup("text/plain"));
	hev_scgi_response_write_header(HEV_SCGI_RESPONSE(task_data->scgi_response), NULL);

	output_stream = hev_scgi_response_get_output_stream(HEV_SCGI_RESPONSE(task_data->scgi_response));
	g_output_stream_write_all(output_stream, message, strlen(message), NULL, NULL, NULL);

	exit(EXIT_FAILURE);
}

