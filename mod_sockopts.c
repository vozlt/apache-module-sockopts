/* ====================================================================
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 2000-2002 The Apache Software Foundation.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The end-user documentation included with the redistribution,
 *    if any, must include the following acknowledgment:
 *       "This product includes software developed by the
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowledgment may appear in the software itself,
 *    if and wherever such third-party acknowledgments normally appear.
 *
 * 4. The names "Apache" and "Apache Software Foundation" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache",
 *    nor may "Apache" appear in their name, without prior written
 *    permission of the Apache Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 *
 * Portions of this software are based upon public domain software
 * originally written at the National Center for Supercomputing Applications,
 * University of Illinois, Urbana-Champaign.
 */


/*
 * @file: mod_sockopts.c
 * @brief: setsockopt setting module
 * @author: YoungJoo.Kim <vozlt@vozlt.com>
 * @version:
 * @date: 20090325
 *
 * mod_sockopts.c : setsockopt setting module
 *
 * This program has been rewritten.
 * original: http://arctic.org/~dean/mod_sockopts/libapache-mod-sockopts-1.0/mod_sockopts.c
 * 
 * This module provides a socket option settings.
 * The following socket option lists are available in this module:
 *
 *          TCP_DEFER_ACCEPT
 *          SO_SNDTIMEO
 *          SO_RCVTIMEO
 *          SO_SNDBUF
 *          SO_RCVBUF
 *
 * - Add module(apxs)
 * [root@root mod_sockopts]# apxs -iac mod_sockopts.c
 *
 * - Configuration(httpd.conf)
 *
 * AddModule mod_sockopts.c
 *
 * <IfModule mod_sockopts.c>
 *      # TCP_DEFER_ACCEPT
 *      SoTcpDeferAccept   20
 *
 *      # SO_SNDTIMEO - not effective(socket is not closed and be continued the data transfering)
 *      # SoSoSndTimeo     5
 *
 *      # SO_RCVTIMEO - not effective(socket is not closed and be continued the data transfering)
 *      # SoSoRcvTimeo     5
 *
 *      # SO_SNDBUF
 *      # SoSoSndBuf       512
 *
 *      # SO_RCVBUF
 *      # SoSoRcvBuf       512
 *
 * </IfModule>
 *
 * To use SO_LINGER:
 *          To try it, add -DUSE_SO_LINGER -DNO_LINGCLOSE to the end of the EXTRA_CFLAGS 
 *          line in your Configuration file, rerun Configure and rebuild the server.(See http_main.c)
 *
 * About TCP_DEFER_ACCEPT timeout:
 *          You can see the following code in net/ipv4/tcp.c in the linux kernel 2.6.x source.
 *
 */

#define CORE_PRIVATE
#include "httpd.h"
#include "http_config.h"
#include "http_log.h"
#include "http_request.h"
#include "http_conf_globals.h"	/* ap_listeners */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

typedef struct {
	/* TCP_DEFER_ACCEPT */
	int tcp_defer_accept_v;
	unsigned int tcp_defer_accept_set:1;

	/* SO_RCVTIMEO */
	struct timeval so_rcvtimeo_v;
	unsigned int so_rcvtimeo_set:1;

	/* SO_SNDTIMEO */
	struct timeval so_sndtimeo_v;
	unsigned int so_sndtimeo_set:1;

	/* SO_SNDBUF */
	int so_sndbuf_v;
	unsigned int so_sndbuf_set:1;

	/* SO_RCVBUF */
	int so_rcvbuf_v;
	unsigned int so_rcvbuf_set:1;

} sockopts_config;

module MODULE_VAR_EXPORT sockopts_module;

/* See the ap_listeners and copy_listeners() function in http_main.c */
static void sockopts_init(server_rec *s, pool *p)
{
	sockopts_config *scfg = ap_get_module_config(s->module_config, &sockopts_module);
	listen_rec *lr;
	socklen_t optlen;

	if (ap_listeners == NULL) {
		return; 
	}
	lr = ap_listeners;
	do {
		/*
		 * http://arctic.org/~dean/mod_sockopts/libapache-mod-sockopts-1.0/mod_sockopts.c
		 * apache initialization sequence is such a mess... we end up
		 * in here once before any listeners have been allocated... so
		 * we don't want to try to setsockopt then.
		 *
		 */
		if (lr->fd != -1) {
#ifdef TCP_DEFER_ACCEPT
			if (scfg->tcp_defer_accept_set) {
				optlen = sizeof(int);
				if (setsockopt(lr->fd, IPPROTO_TCP, TCP_DEFER_ACCEPT, (const void *)&scfg->tcp_defer_accept_v, optlen) < 0) {
					ap_log_error(APLOG_MARK, APLOG_WARNING, s, "failed to setsockopt(IPPROTO_TCP, TCP_DEFER_ACCEPT)");
				}
			}
#endif
			if (scfg->so_sndtimeo_set) {
				optlen = sizeof(struct timeval);
				if (setsockopt(lr->fd, SOL_SOCKET, SO_SNDTIMEO, (const void *)&scfg->so_sndtimeo_v, optlen) < 0) {
					ap_log_error(APLOG_MARK, APLOG_WARNING, s, "failed to setsockopt(SOL_SOCKET, SO_SNDTIMEO)");
				}
			}
			if (scfg->so_rcvtimeo_set) {
				optlen = sizeof(struct timeval);
				if (setsockopt(lr->fd, SOL_SOCKET, SO_RCVTIMEO, (const void *)&scfg->so_rcvtimeo_v, optlen) < 0) {
					ap_log_error(APLOG_MARK, APLOG_WARNING, s, "failed to setsockopt(SOL_SOCKET, SO_RCVTIMEO)");
				}
			}
			if (scfg->so_sndbuf_set) {
				optlen = sizeof(int);
				if (setsockopt(lr->fd, SOL_SOCKET, SO_SNDBUF, (const void *)&scfg->so_sndbuf_v, optlen) < 0) {
					ap_log_error(APLOG_MARK, APLOG_WARNING, s, "failed to setsockopt(SOL_SOCKET, SO_SNDBUF)");
				}
			}
			if (scfg->so_rcvbuf_set) {
				optlen = sizeof(int);
				if (setsockopt(lr->fd, SOL_SOCKET, SO_RCVBUF, (const void *)&scfg->so_rcvbuf_v, optlen) < 0) {
					ap_log_error(APLOG_MARK, APLOG_WARNING, s, "failed to setsockopt(SOL_SOCKET, SO_RCVBUF)");
				}
			}
		}
		lr = lr->next;
	} while (lr && lr != ap_listeners);

} 

static void *sockopts_create_server_config(pool *p, server_rec *s)
{
	sockopts_config *scfg = (sockopts_config *)ap_pcalloc(p, sizeof(sockopts_config));
	return (void *) scfg;
}

static void *sockopts_merge_server_config(pool *p, void *_parent, void *_child)
{
	sockopts_config *parent = (sockopts_config *) _parent;
	sockopts_config *child = (sockopts_config *) _child;
	sockopts_config *scfg = (sockopts_config *)ap_palloc(p, sizeof(*scfg));

	*scfg = *parent;
	if (child->tcp_defer_accept_set) {
		scfg->tcp_defer_accept_set = 1;
		scfg->tcp_defer_accept_v = child->tcp_defer_accept_v;
	}
	if (child->so_sndtimeo_set) {
		scfg->so_sndtimeo_set = 1;
		scfg->so_sndtimeo_v.tv_sec = child->so_sndtimeo_v.tv_sec;
		scfg->so_sndtimeo_v.tv_usec = child->so_sndtimeo_v.tv_usec;
	}
	if (child->so_rcvtimeo_set) {
		scfg->so_rcvtimeo_set = 1;
		scfg->so_rcvtimeo_v.tv_sec = child->so_rcvtimeo_v.tv_sec;
		scfg->so_rcvtimeo_v.tv_usec = child->so_rcvtimeo_v.tv_usec;
	}
	if (child->so_sndbuf_set) {
		scfg->so_sndbuf_set = 1;
		scfg->so_sndbuf_v = child->so_sndbuf_v;
	}
	if (child->so_rcvbuf_set) {
		scfg->so_rcvbuf_set = 1;
		scfg->so_rcvbuf_v = child->so_rcvbuf_v;
	}

	return (void *) scfg;
}

static const char *set_tcp_defer_accept(cmd_parms *cmd, void *dcfg, char *value)
{
	sockopts_config *scfg = ap_get_module_config(cmd->server->module_config, &sockopts_module);
	
	const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
	if (err != NULL) {
		return err;
	}
	
	scfg->tcp_defer_accept_v = atoi(value);
	scfg->tcp_defer_accept_set = 1;
	return NULL;
}

static const char *set_so_sndtimeo(cmd_parms *cmd, void *dcfg, char *value)
{
	sockopts_config *scfg = ap_get_module_config(cmd->server->module_config, &sockopts_module);
	
	const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
	if (err != NULL) {
		return err;
	}
	
	scfg->so_sndtimeo_v.tv_sec = atoi(value);
	scfg->so_sndtimeo_v.tv_usec = 0;
	scfg->so_sndtimeo_set = 1;
	return NULL;
}

static const char *set_so_rcvtimeo(cmd_parms *cmd, void *dcfg, char *value)
{
	sockopts_config *scfg = ap_get_module_config(cmd->server->module_config, &sockopts_module);
	
	const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
	if (err != NULL) {
		return err;
	}
	
	scfg->so_rcvtimeo_v.tv_sec = atoi(value);
	scfg->so_rcvtimeo_v.tv_usec = 0;
	scfg->so_rcvtimeo_set = 1;
	return NULL;
}

static const char *set_so_sndbuf(cmd_parms *cmd, void *dcfg, char *value)
{
	sockopts_config *scfg = ap_get_module_config(cmd->server->module_config, &sockopts_module);
	
	const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
	if (err != NULL) {
		return err;
	}
	/* linux minimum buffer size : 256 and default set of setsockopt is (setvalue * 2) */
	scfg->so_sndbuf_v = atoi(value);	
	scfg->so_sndbuf_set = 1;	
	return NULL;
}

static const char *set_so_rcvbuf(cmd_parms *cmd, void *dcfg, char *value)
{
	sockopts_config *scfg = ap_get_module_config(cmd->server->module_config, &sockopts_module);
	
	const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
	if (err != NULL) {
		return err;
	}
	/* linux minimum buffer size : 256 and default set of setsockopt is (setvalue * 2) */
	scfg->so_rcvbuf_v = atoi(value);	
	scfg->so_rcvbuf_set = 1;	
	return NULL;
}

/* See http_config.h */
static const command_rec sockopts_cmds[] = {
	{   
		"SoTcpDeferAccept",
		set_tcp_defer_accept,	/* user function pointer */
		NULL,
		RSRC_CONF,				/* *.conf outside <Directory> or <Location> */
		TAKE1,					/* one argument only */
		"TCP_DEFER_ACCEPT (You can see the man tcp(7))"
	},

	{   
		"SoSoSndTimeo",
		set_so_sndtimeo,
		NULL,
		RSRC_CONF,
		TAKE1,
		"SO_SNDTIMEO (You can see the man socket(7))"
	},

	{   
		"SoSoRcvTimeo",
		set_so_rcvtimeo,
		NULL,
		RSRC_CONF,
		TAKE1,
		"SO_RCVTIMEO (You can see the man socket(7))"
	},

	{   
		"SoSoRcvBuf",
		set_so_rcvbuf,
		NULL,
		RSRC_CONF,
		TAKE1,
		"SO_RCVBUF (You can see the man socket(7))"
	},

	{   
		"SoSoSndBuf",
		set_so_sndbuf,
		NULL,
		RSRC_CONF,
		TAKE1,
		"SO_SNDBUF (You can see the man socket(7))"
	},

	{NULL}
};

module MODULE_VAR_EXPORT sockopts_module =
{
	STANDARD_MODULE_STUFF,
	sockopts_init,						/* module initializer */
	NULL,								/* per-directory config creator */
	NULL,								/* dir config merger */
	sockopts_create_server_config,		/* server config creator */
	sockopts_merge_server_config,		/* server config merger */
	sockopts_cmds,						/* command table */
	NULL,								/* [9] list of handlers */
	NULL,								/* [2] filename-to-URI translation */
	NULL,								/* [5] check/validate user_id */
	NULL,								/* [6] check user_id is valid *here* */
	NULL,								/* [4] check access by host address */
	NULL,								/* [7] MIME type checker/setter */
	NULL,								/* [8] fixups */
	NULL,								/* [10] logger */
#if MODULE_MAGIC_NUMBER >= 19970103
	NULL,								/* [3] header parser */
#endif
#if MODULE_MAGIC_NUMBER >= 19970719
	NULL,								/* process initializer */
#endif
#if MODULE_MAGIC_NUMBER >= 19970728
	NULL,								/* process exit/cleanup */
#endif
#if MODULE_MAGIC_NUMBER >= 19970902
	NULL								/* [1] post read_request handling */
#endif
};

