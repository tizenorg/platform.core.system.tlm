/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm (Tiny Login Manager)
 *
 * Copyright (C) 2015 Samsung Electronics Co.
 *
 * Contact: Youmin Ha <youmin.ha@samsung.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <cynara-client.h>
#include <cynara-creds-gdbus.h>
#include <cynara-creds-socket.h>
#include <cynara-session.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tlm-cynara-privilege-checker.h"
#include "tlm-log.h"

#define _PRIVILEGE_TLM_API "http://tizen.org/privilege/login"


static int
_get_socket_fd_from_g_dbus_method_invocation(GDBusMethodInvocation *invocation)
{
    // NOTE: GDBusMethodInvocation -> GDBusConnection -> GIOStream ->
    //       GSocketConnection -> GSocket -> socket fd
    GDBusConnection *conn = g_dbus_method_invocation_get_connection(invocation);
    if (!conn) return -1;

    // NOTE: GSocketConnection is a GIOStream, so here it is casted.
    GSocketConnection *gsconn = G_SOCKET_CONNECTION(
        g_dbus_connection_get_stream(conn));
    if (!gsconn) return -1;

    GSocket *gsock = g_socket_connection_get_socket(gsconn);
    if (!gsock) return -1;

    int fd = g_socket_get_fd(gsock);

    return fd;
}

/*
// is this needed?
static char *
_get_smack_label_from_fd(int fd) {
    typedef char SmackLabel[SMACK_LABEL_LEN];
    SmackLabel smack_label;
    socklen_t smack_label_len = sizeof(SmackLabel);
    int err;

    err = getsockopt(fd, SOL_SOCKET, SO_PEERSEC, &smack_label, &smack_label_len);
    if (err) return NULL;
}
*/

gboolean
tlm_cynara_privilege_checker_check_privilege_from_g_dbus_method_invocation(
    GDBusMethodInvocation *invocation)
{
    gboolean ret = FALSE;
    cynara_configuration *cy_conf = NULL;
    cynara *cy = NULL;
    // required values
    char *client = NULL, *client_session = NULL, *user = NULL;
    pid_t pid;
    int fd;
    int err;

    // 0. Get fd
    fd = _get_socket_fd_from_g_dbus_method_invocation(invocation);
    if (-1 == fd) goto CLEANUP_RETURN;

    // 1. Prepare cynara object
    if (CYNARA_API_SUCCESS != cynara_configuration_create(&cy_conf))
            goto CLEANUP_RETURN;
    if (CYNARA_API_SUCCESS != cynara_initialize(&cy, cy_conf))
            goto CLEANUP_RETURN;

    // 2. Prepare required values for the cynara query:
    // client
    err = cynara_creds_socket_get_client(fd, CLIENT_METHOD_DEFAULT,
            (gchar**) &client);
    if (CYNARA_API_SUCCESS != err) goto CLEANUP_RETURN;
    // client_session
    if (CYNARA_API_SUCCESS != cynara_creds_socket_get_pid(fd, &pid))
        goto CLEANUP_RETURN;
    client_session = cynara_session_from_pid(pid);
    // user
    if (CYNARA_API_SUCCESS != cynara_creds_socket_get_user(fd,
            USER_METHOD_DEFAULT, (gchar**) &user)) goto CLEANUP_RETURN;


    // 3. Check the privilege
    err = cynara_simple_check(cy, client, client_session, user,
            _PRIVILEGE_TLM_API);
    if (CYNARA_API_ACCESS_ALLOWED == err) {
        ret = TRUE;
    }
    DBG("cynara:%s %s %s, err=%d\n",
        client, client_session, user, err);

CLEANUP_RETURN:
    g_free((gchar*) client);
    if (client_session) free(client_session);
    g_free((gchar*) user);
    if (cy) cynara_finish(cy);
    if (cy_conf) cynara_configuration_destroy(cy_conf);

    return ret;
}

