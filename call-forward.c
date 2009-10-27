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


gchar *get_divert(enum ss_diverts divert_type)
{
  DBusError dbus_error;
  dbus_uint32_t d_divert_type = divert_type;
  dbus_bool_t divert_set;
  const char *p_num;
  gchar *return_string;

  dbus_error_init(&dbus_error);

  DBusMessage *msg = dbus_message_new_method_call("com.nokia.csd.SS",
						  "/com/nokia/csd/ss",
						  "com.nokia.csd.SS",
						  "DivertCheck");
  if (!msg) {
    g_error("%s: Error creating DBus message",__func__);
    return NULL;
  }

  if (!dbus_message_append_args(msg,
				DBUS_TYPE_UINT32, &d_divert_type,
				DBUS_TYPE_INVALID)) {
    g_error("%s: Error adding DBus arguments",__func__);
    return NULL;
  }

  DBusMessage *reply;
  reply = dbus_connection_send_with_reply_and_block(dbus_system,
						    msg,
						    -1,
						    &dbus_error);
  if (!reply) {
    g_debug("%s: DivertCheck(%d) error %s: %s",__func__,divert_type,dbus_error.name, dbus_error.message);
    return NULL;
  }
  dbus_message_unref(msg);

  /* The first reply arg says whether a divert is set */
  if (!dbus_message_get_args(reply, &dbus_error,
			     DBUS_TYPE_BOOLEAN, &divert_set,
			     DBUS_TYPE_INVALID)) {
    g_error("%s: DivertCheck(%d) reply arg 1 error %s: %s",__func__,divert_type,dbus_error.name, dbus_error.message);
    return NULL;
  }
  if (!divert_set) return NULL;

  /* If the divert is set the second arg will be present */
  if (!dbus_message_get_args(reply, &dbus_error,
			     DBUS_TYPE_BOOLEAN, &divert_set,
			     DBUS_TYPE_STRING, &p_num,
			     DBUS_TYPE_INVALID)) {
    g_error("%s: DivertCheck(%d) reply arg 2 error %s: %s",__func__,divert_type,dbus_error.name, dbus_error.message);
    return NULL;
  }

  return_string = g_strdup(p_num);

  dbus_message_unref(reply);

  return return_string;
}

int main (int argc, char *argv[])
{
  DBusError dbus_error;

  hildon_gtk_init(&argc, &argv);

  dbus_g_thread_init();

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

  HildonProgram *program = hildon_program_get_instance ();
  HildonStackableWindow *main_window = HILDON_STACKABLE_WINDOW (hildon_stackable_window_new());
  hildon_program_add_window (program, HILDON_WINDOW (main_window));
  g_signal_connect (G_OBJECT (main_window), "delete_event",
                    G_CALLBACK (gtk_main_quit), NULL);

  GtkBox *main_box = GTK_BOX (gtk_vbox_new (FALSE, 0));
  gtk_container_add (GTK_CONTAINER (main_window), GTK_WIDGET (main_box));

  g_print("Unconditional divert = %s\n", get_divert(SS_DIVERT_ALL));
  g_print("Unconditional divert = %s\n", get_divert(SS_DIVERT_BUSY));

  gtk_widget_show_all(GTK_WIDGET(main_window));
  gtk_main();
  return 0;
}
