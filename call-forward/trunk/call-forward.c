/*
 * call-forward - Manage call forwarding settings in a Maemo environment
 * Copyright (C) 2009  Graham R. Cobb <g+770@cobb.uk.net>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 * 
 */

#include <config.h>
#include <glib.h>
#define DBUS_API_SUBJECT_TO_CHANGE 1
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus.h>
#include <hildon/hildon.h>
#include <libosso.h>

#define PACKAGE_DBUS_NAME "net.uk.cobb.call-forward"

DBusConnection *dbus_system, *dbus_session;
osso_context_t* osso_ctx;
dbus_int32_t ss_dbus_pending_slot_cb = -1;

/* Diversion types */
enum ss_diverts {
  SS_ALL_DIVERTS, /* Set only -- sets all diverts */
  SS_DIVERT_ALL, /* Unconditional */
  SS_DIVERT_BUSY, /* Divert on Busy */
  SS_DIVERT_NO_REPLY, /* Divert on no reply */
  SS_DIVERT_NO_REACH, /* Divert on not reachable */
  SS_DIVERT_NO_AVAIL /* Set only -- sets all of SS_DIVERT_BUSY, SS_DIVERT_NO_REPLY and SS_DIVERT_NO_REACH */
};
enum ss_barrings {
  SS_ALL_BARRINGS = 6, /* Set only -- sets all barrings */
  SS_BARR_ALL_OUT, /* Barring All Outgoing */
  SS_BARR_OUT_INTER, /* Barring Outgoing International */
  SS_BARR_OUT_INTER_EXC_HOME, /* Barring Outgoing International except Home Country */
  SS_BARR_ALL_IN, /* Barring All Incoming */
  SS_BARR_ALL_IN_ROAM /* Barring All Incoming when Roaming */
};

typedef void (*ss_divert_check_reply)(void *param, gboolean divert_set, gchar *divert_number, DBusError *dbus_error);

void ss_get_divert_reply(DBusPendingCall *pending, void *user_data) {
  DBusMessage *reply;
  DBusError dbus_error;
  dbus_bool_t divert_set = FALSE;
  gchar *divert_number = NULL;
  ss_divert_check_reply callback;

  dbus_error_init(&dbus_error);

  callback = (ss_divert_check_reply) dbus_pending_call_get_data(pending, ss_dbus_pending_slot_cb);

  reply = dbus_pending_call_steal_reply(pending);

  dbus_pending_call_unref(pending);

  if (!reply) {
    g_error("%s: Error stealing DivertCheck reply",__func__);
    dbus_set_error(&dbus_error, PACKAGE_DBUS_NAME ".Error.StealReply", "Error stealing DivertCheck reply");
    goto exit;
  }

  /* Check for an error response */
  if (dbus_message_get_type(reply) != DBUS_MESSAGE_TYPE_METHOD_RETURN) {
    char *p_error = "Unknown response";

    /* Let's try to check the first argument to see if it is a string.
       If so, it might be useful to include in the error */
    dbus_message_get_args(reply, NULL,
			  DBUS_TYPE_STRING, &p_error,
			  DBUS_TYPE_INVALID);

    dbus_set_error(&dbus_error, PACKAGE_DBUS_NAME ".Error.ErrorReply", "DivertCheck: %s", p_error);
    g_debug("%s: %s - %s", __func__, dbus_error.name, dbus_error.message);
    goto exit;
  }

  /* The first reply arg says whether a divert is set */
  if (!dbus_message_get_args(reply, &dbus_error,
			     DBUS_TYPE_BOOLEAN, &divert_set,
			     DBUS_TYPE_INVALID)) {
    g_error("%s: DivertCheck reply arg 1 error %s: %s",__func__, dbus_error.name, dbus_error.message);
    goto exit_unref_reply;
  }

  if (divert_set) {
    const char *p_num;
    /* If the divert is set the second arg will be present */
    if (!dbus_message_get_args(reply, &dbus_error,
			       DBUS_TYPE_BOOLEAN, &divert_set,
			       DBUS_TYPE_STRING, &p_num,
			       DBUS_TYPE_INVALID)) {
      g_error("%s: DivertCheck reply arg 2 error %s: %s",__func__, dbus_error.name, dbus_error.message);
      goto exit_unref_reply;
    }
    divert_number = g_strdup(p_num);
  }

 exit_unref_reply:
  dbus_message_unref(reply);
 exit:
  (*callback)(user_data, divert_set, divert_number, &dbus_error);

  dbus_error_free(&dbus_error);
  return;
}

gboolean ss_get_divert(enum ss_diverts divert_type, ss_divert_check_reply reply_callback, void* user_data)
{
  DBusPendingCall *pending_return;
  dbus_uint32_t d_divert_type = divert_type;

  DBusMessage *msg = dbus_message_new_method_call("com.nokia.csd.SS",
						  "/com/nokia/csd/ss",
						  "com.nokia.csd.SS",
						  "DivertCheck");
  if (!msg) {
    g_error("%s: Error creating DBus message",__func__);
    return FALSE;
  }

  if (!dbus_message_append_args(msg,
				DBUS_TYPE_UINT32, &d_divert_type,
				DBUS_TYPE_INVALID)) {
    g_error("%s: Error adding DBus arguments",__func__);
    dbus_message_unref(msg);
    return FALSE;
  }

  if (!dbus_connection_send_with_reply(dbus_system,
					  msg,
					  &pending_return,
				       -1)) {
    g_error("%s: Error sending DBus message",__func__);
    dbus_message_unref(msg);
    return FALSE;
  }

  dbus_message_unref(msg);

  if (!dbus_pending_call_set_data(pending_return, ss_dbus_pending_slot_cb, (void *)reply_callback, NULL)) {
    g_error("%s: Error setting reply notify",__func__);
    dbus_pending_call_unref(pending_return);
    return FALSE;
  }

  if (!dbus_pending_call_set_notify(pending_return, ss_get_divert_reply, user_data, NULL)) {
    g_error("%s: Error setting reply notify",__func__);
    dbus_pending_call_unref(pending_return);
    return FALSE;
  }

  return TRUE;
}

void got_divert(void *param, gboolean divert_set, gchar *divert_number, DBusError *dbus_error) {
  if (dbus_error_is_set(dbus_error)) {
    g_error("%s: Error getting existing divert information: %s - %s", __func__, dbus_error->name, dbus_error->message);
    return;
  }

  if (divert_set) {
    g_print("%s = %s\n", (char *)param, divert_number);
  } else {
    g_print("%s is not set (%p)\n", (char *)param, divert_number);
  }

  g_free(divert_number);

  return;
}

int main (int argc, char *argv[])
{
  DBusError dbus_error;

  hildon_gtk_init(&argc, &argv);

  dbus_error_init(&dbus_error);

  dbus_system = dbus_bus_get(DBUS_BUS_SYSTEM, &dbus_error);
  if (dbus_error_is_set(&dbus_error)) {
    g_error("DBus System Connection error %s: %s", dbus_error.name, dbus_error.message);
    return 1;
  }
  dbus_error_free(&dbus_error);
  dbus_connection_setup_with_g_main(dbus_system, NULL);

  dbus_session = dbus_bus_get(DBUS_BUS_SESSION, &dbus_error);
  if (dbus_error_is_set(&dbus_error)) {
    g_error("DBus Session Connection error %s: %s", dbus_error.name, dbus_error.message);
    return 1;
  }
  dbus_error_free(&dbus_error);
  dbus_connection_setup_with_g_main(dbus_session, NULL);

  osso_ctx = osso_initialize_with_connections (PACKAGE_DBUS_NAME, "0.1", dbus_system, dbus_session);
  if (!osso_ctx) {
    g_error("Error initializing the osso context");
    return 1;
  }

  if (!dbus_pending_call_allocate_data_slot(&ss_dbus_pending_slot_cb)) {
    g_error("Error allocating dbus pending call slot for callback");
    return 1;
  }


  HildonProgram *program = hildon_program_get_instance ();
  HildonStackableWindow *main_window = HILDON_STACKABLE_WINDOW (hildon_stackable_window_new());
  hildon_program_add_window (program, HILDON_WINDOW (main_window));
  g_signal_connect (G_OBJECT (main_window), "delete_event",
                    G_CALLBACK (gtk_main_quit), NULL);

  GtkWidget *pannable_area = hildon_pannable_area_new ();
  gtk_container_add (GTK_CONTAINER (main_window), pannable_area);


  GtkBox *main_box = GTK_BOX (gtk_vbox_new (FALSE, 0));
  hildon_pannable_area_add_with_viewport (
					  HILDON_PANNABLE_AREA (pannable_area), GTK_WIDGET (main_box));

  ss_get_divert(SS_DIVERT_ALL, got_divert, "Unconditional divert");
  //  ss_get_divert(SS_DIVERT_BUSY, got_divert, "Divert on busy");

  gtk_widget_show_all(GTK_WIDGET(main_window));
  gtk_main();
  return 0;
}
