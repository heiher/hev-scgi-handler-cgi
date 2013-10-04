/*
 ============================================================================
 Name        : hev-scgi-handler-cgi.h
 Author      : Heiher <root@heiher.info>
 Version     : 0.0.8
 Copyright   : Copyright (C) 2013 everyone.
 Description : 
 ============================================================================
 */

#ifndef __HEV_SCGI_HANDLER_CGI_H__
#define __HEV_SCGI_HANDLER_CGI_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define HEV_REGISTER_TYPE_SCGI_HANDLER_CGI(module) (hev_scgi_handler_cgi_reg_type(module))
#define HEV_TYPE_SCGI_HANDLER_CGI	(hev_scgi_handler_cgi_get_type())
#define HEV_SCGI_HANDLER_CGI(obj)	(G_TYPE_CHECK_INSTANCE_CAST((obj), HEV_TYPE_SCGI_HANDLER_CGI, HevSCGIHandlerCGI))
#define HEV_IS_SCGI_HANDLER_CGI(obj)	(G_TYPE_CHECK_INSTANCE_TYPE((obj), HEV_TYPE_SCGI_HANDLER_CGI))
#define HEV_SCGI_HANDLER_CGI_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), HEV_TYPE_SCGI_HANDLER_CGI, HevSCGIHandlerCGIClass))
#define HEV_IS_SCGI_HANDLER_CGI_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), HEV_TYPE_SCGI_HANDLER_CGI))
#define HEV_SCGI_HANDLER_CGI_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), HEV_TYPE_SCGI_HANDLER_CGI, HevSCGIHandlerCGIClass))

typedef struct _HevSCGIHandlerCGI HevSCGIHandlerCGI;
typedef struct _HevSCGIHandlerCGIClass HevSCGIHandlerCGIClass;

struct _HevSCGIHandlerCGI
{
	GObject parent_instance;
};

struct _HevSCGIHandlerCGIClass
{
	GObjectClass parent_class;
};

void hev_scgi_handler_cgi_reg_type(GTypeModule *module);
GType hev_scgi_handler_cgi_get_type(void);

G_END_DECLS

#endif /* __HEV_SCGI_HANDLER_CGI_H__ */

