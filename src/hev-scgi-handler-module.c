/*
 ============================================================================
 Name        : hev-scgi-handler-module.c
 Author      : Heiher <admin@heiher.info>
 Version     : 0.0.1
 Copyright   : Copyright (C) 2011 everyone.
 Description : 
 ============================================================================
 */

#include <glib-object.h>
#include <gmodule.h>

#include "hev-scgi-handler-cgi.h"

G_MODULE_EXPORT GType hev_scgi_handler_module_get_handler_type(GTypeModule *module)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	HEV_REGISTER_TYPE_SCGI_HANDLER_CGI(module);

	return HEV_TYPE_SCGI_HANDLER_CGI;
}

