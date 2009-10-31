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
#include <hildon/hildon-entry.h>
#include <libosso.h>
#include <libintl.h>
#define _(String) gettext (String)
#include <locale.h>
#include <string.h>

#define PACKAGE_DBUS_NAME "net.uk.cobb.call_forward"

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
#define SS_DIVERT_MIN SS_ALL_DIVERTS
#define SS_DIVERT_MAX SS_DIVERT_NO_AVAIL
enum ss_barrings {
  SS_ALL_BARRINGS = 6, /* Set only -- sets all barrings */
  SS_BARR_ALL_OUT, /* Barring All Outgoing */
  SS_BARR_OUT_INTER, /* Barring Outgoing International */
  SS_BARR_OUT_INTER_EXC_HOME, /* Barring Outgoing International except Home Country */
  SS_BARR_ALL_IN, /* Barring All Incoming */
  SS_BARR_ALL_IN_ROAM /* Barring All Incoming when Roaming */
};
#define SS_BARR_MIN SS_ALL_BARRINGS
#define SS_BARR_MAX SS_BARR_ALL_IN_ROAM

typedef void (*ss_divert_check_reply)(void *param, gboolean divert_set, gchar *divert_number, DBusError *dbus_error);
typedef void (*ss_divert_set_reply)(void *param, DBusError *dbus_error);
typedef void (*ss_divert_cancel_reply)(void *param, DBusError *dbus_error);

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
    goto exit_unref_reply;
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

void ss_set_divert_reply(DBusPendingCall *pending, void *user_data) {
  DBusMessage *reply;
  DBusError dbus_error;
  ss_divert_set_reply callback;

  dbus_error_init(&dbus_error);

  callback = (ss_divert_set_reply) dbus_pending_call_get_data(pending, ss_dbus_pending_slot_cb);

  reply = dbus_pending_call_steal_reply(pending);

  dbus_pending_call_unref(pending);

  if (!reply) {
    g_error("%s: Error stealing DivertActivate reply",__func__);
    dbus_set_error(&dbus_error, PACKAGE_DBUS_NAME ".Error.StealReply", "Error stealing DivertActivate reply");
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
    goto exit_unref_reply;
  }

 exit_unref_reply:
  dbus_message_unref(reply);
 exit:
  (*callback)(user_data, &dbus_error);

  dbus_error_free(&dbus_error);
  return;
}

gboolean ss_set_divert(enum ss_diverts divert_type, char *number, ss_divert_set_reply reply_callback, void* user_data)
{
  DBusPendingCall *pending_return;
  dbus_uint32_t d_divert_type = divert_type;
  dbus_uint32_t divert_timer = 20; /* Seconds */

  g_print("%s: Set divert %d to %s\n", __func__, divert_type, number);

  DBusMessage *msg = dbus_message_new_method_call("com.nokia.csd.SS",
						  "/com/nokia/csd/ss",
						  "com.nokia.csd.SS",
						  "DivertActivate");
  if (!msg) {
    g_error("%s: Error creating DBus message",__func__);
    return FALSE;
  }

  if (!dbus_message_append_args(msg,
				DBUS_TYPE_UINT32, &d_divert_type,
				DBUS_TYPE_STRING, &number,
				DBUS_TYPE_UINT32, &divert_timer,
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

  if (!dbus_pending_call_set_notify(pending_return, ss_set_divert_reply, user_data, NULL)) {
    g_error("%s: Error setting reply notify",__func__);
    dbus_pending_call_unref(pending_return);
    return FALSE;
  }

  return TRUE;
}

void ss_cancel_divert_reply(DBusPendingCall *pending, void *user_data) {
  DBusMessage *reply;
  DBusError dbus_error;
  ss_divert_cancel_reply callback;

  dbus_error_init(&dbus_error);

  callback = (ss_divert_cancel_reply) dbus_pending_call_get_data(pending, ss_dbus_pending_slot_cb);

  reply = dbus_pending_call_steal_reply(pending);

  dbus_pending_call_unref(pending);

  if (!reply) {
    g_error("%s: Error stealing DivertCancel reply",__func__);
    dbus_set_error(&dbus_error, PACKAGE_DBUS_NAME ".Error.StealReply", "Error stealing DivertCancel reply");
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
    goto exit_unref_reply;
  }

 exit_unref_reply:
  dbus_message_unref(reply);
 exit:
  (*callback)(user_data, &dbus_error);

  dbus_error_free(&dbus_error);
  return;
}


gboolean ss_cancel_divert(enum ss_diverts divert_type, ss_divert_cancel_reply reply_callback, void* user_data)
{
  DBusPendingCall *pending_return;
  dbus_uint32_t d_divert_type = divert_type;

  g_print("%s: Cancel divert %d\n", __func__, divert_type);

  DBusMessage *msg = dbus_message_new_method_call("com.nokia.csd.SS",
						  "/com/nokia/csd/ss",
						  "com.nokia.csd.SS",
						  "DivertCancel");
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

  if (!dbus_pending_call_set_notify(pending_return, ss_cancel_divert_reply, user_data, NULL)) {
    g_error("%s: Error setting reply notify",__func__);
    dbus_pending_call_unref(pending_return);
    return FALSE;
  }

  return TRUE;
}

struct divert_entry {
  GtkWindow *window;
  GtkBox *hbox;
  GtkWidget *button;
  GtkWidget *entry;
  char *label;
  char *number;
  enum ss_diverts divert_type;
  gboolean divert_set;
  gboolean update_needed;
  gboolean update_done;
};

struct divert_entry diverts[SS_DIVERT_MAX - SS_DIVERT_MIN + 1];

void got_divert(void *param, gboolean divert_set, gchar *divert_number, DBusError *dbus_error)
{
  int i = (int) param - SS_DIVERT_MIN;

  g_assert(diverts[i].update_needed);

  diverts[i].update_needed = FALSE;

  if (dbus_error_is_set(dbus_error)) {
    GtkWidget *note;
    gchar *error;
    error = g_strdup_printf(_("Error getting existing divert information: %s - %s"), dbus_error->name, dbus_error->message);
    g_debug("%s: %s", __func__, error);
    note = hildon_note_new_information (diverts[i].window, error);
    gtk_dialog_run (GTK_DIALOG (note));
    gtk_object_destroy (GTK_OBJECT (note));
    g_free(error);
  } else {
    gtk_widget_set_sensitive(GTK_WIDGET(diverts[i].hbox), TRUE);

    hildon_check_button_set_active(HILDON_CHECK_BUTTON(diverts[i].button), divert_set);

    if (divert_set) {
      gtk_entry_set_text(GTK_ENTRY(diverts[i].entry), divert_number);
    }

    g_free(diverts[i].number);
    diverts[i].number = divert_number;
    diverts[i].divert_set = divert_set;
  }

  for (i=0; i + SS_DIVERT_MIN <= SS_DIVERT_MAX; i++) {
    if (diverts[i].update_needed) {
      ss_get_divert(diverts[i].divert_type, got_divert, (void *) diverts[i].divert_type);
      break;
    }
  }

  return;
}

int add_divert_row(GtkWindow *window, GtkContainer *box, char *label, enum ss_diverts divert_type) {
  int i = divert_type - SS_DIVERT_MIN;

  diverts[i].window = window;

  diverts[i].hbox = GTK_BOX (gtk_hbox_new (FALSE, 0));
  gtk_container_add (box, GTK_WIDGET(diverts[i].hbox));
  gtk_widget_set_sensitive(GTK_WIDGET(diverts[i].hbox), FALSE);

  diverts[i].button = hildon_check_button_new(HILDON_SIZE_AUTO);
  gtk_button_set_label (GTK_BUTTON (diverts[i].button), label);
  gtk_container_add (GTK_CONTAINER(diverts[i].hbox), diverts[i].button);

  diverts[i].entry = hildon_entry_new(HILDON_SIZE_AUTO);
  hildon_entry_set_placeholder (HILDON_ENTRY (diverts[i].entry),
				_("Number"));
  hildon_gtk_entry_set_input_mode (GTK_ENTRY (diverts[i].entry), HILDON_GTK_INPUT_MODE_TELE);
  gtk_container_add (GTK_CONTAINER(diverts[i].hbox), diverts[i].entry);

  diverts[i].label = label;
  diverts[i].number = NULL;
  diverts[i].divert_type = divert_type;
  diverts[i].update_needed = TRUE;
  diverts[i].update_done = FALSE;

  return i;
}

int find_next_changed_divert(int i)
{
  if (i + SS_DIVERT_MIN > SS_DIVERT_MAX) return -1; /* No more */

  for (; i + SS_DIVERT_MIN <= SS_DIVERT_MAX; i++) {
    /* FIXME: can't test for sensitivity in this old version of GTK */
#if 0
    if (!gtk_widget_is_sensitive(GTK_WIDGET(diverts[i].hbox))) continue; /* Ignore insensitive */
#endif

    if (diverts[i].update_done) continue; /* Skip entries which have already been updated */

    if (diverts[i].divert_set) {
      if (!hildon_check_button_get_active(HILDON_CHECK_BUTTON(diverts[i].button))) return i;
      if (strcmp(diverts[i].number, gtk_entry_get_text(GTK_ENTRY(diverts[i].entry))) != 0) return i;
    } else {
      if (hildon_check_button_get_active(HILDON_CHECK_BUTTON(diverts[i].button))) return i;
    }
  }

  return -1;
}

void update_manual_divert(int i);

void manual_divert_set(void *param, DBusError *dbus_error)
{
  int i = (int) param - SS_DIVERT_MIN;
  GtkWindow *window = diverts[i].window;

  diverts[i].update_done = TRUE;

  if (dbus_error_is_set(dbus_error)) {
    GtkWidget *note;
    gchar *error;
    error = g_strdup_printf(_("Error changing divert information: %s - %s"), dbus_error->name, dbus_error->message);
    g_debug("%s: %s", __func__, error);
    note = hildon_note_new_information (diverts[i].window, error);
    gtk_dialog_run (GTK_DIALOG (note));
    gtk_object_destroy (GTK_OBJECT (note));
    g_free(error);
  }

  i = find_next_changed_divert(++i);

  if (i>=0) update_manual_divert(i);
  else gtk_object_destroy (GTK_OBJECT(window));

}

void update_manual_divert(int i)
{
  if (hildon_check_button_get_active(HILDON_CHECK_BUTTON(diverts[i].button)))
    ss_set_divert(diverts[i].divert_type, 
		  (char *)gtk_entry_get_text(GTK_ENTRY(diverts[i].entry)), 
		  manual_divert_set, (void *) diverts[i].divert_type);
  else
    ss_cancel_divert(diverts[i].divert_type,
		  manual_divert_set, (void *) diverts[i].divert_type);

  gtk_widget_set_sensitive(GTK_WIDGET(diverts[i].hbox), FALSE);
}

gboolean manual_diverts_query (GtkWidget *widget, GdkEvent  *event, gpointer   user_data)
{
  int i;

  i = find_next_changed_divert(0);

  if (i < 0) return FALSE;

  GtkWidget *note;
  note = hildon_note_new_confirmation (GTK_WINDOW(widget), 
				       _("Divert information has changed.\nDo you want to perform the changes?"));
  gint response = gtk_dialog_run(GTK_DIALOG(note));
  gtk_object_destroy (GTK_OBJECT (note));

  if (response == GTK_RESPONSE_OK) {
    update_manual_divert(i);
    return TRUE; /* Cancel the destroy for now */
  }

  return FALSE;
}

void show_manual_diverts(void)
{
  HildonStackableWindow *window = HILDON_STACKABLE_WINDOW (hildon_stackable_window_new());

  GtkWidget *pannable_area = hildon_pannable_area_new ();
  gtk_container_add (GTK_CONTAINER (window), pannable_area);


  GtkBox *main_box = GTK_BOX (gtk_vbox_new (FALSE, 0));
  hildon_pannable_area_add_with_viewport (
					  HILDON_PANNABLE_AREA (pannable_area), GTK_WIDGET (main_box));

  add_divert_row(GTK_WINDOW(window), GTK_CONTAINER(main_box), _("Unconditional"), SS_DIVERT_ALL);
  add_divert_row(GTK_WINDOW(window), GTK_CONTAINER(main_box), _("Busy"), SS_DIVERT_BUSY);
  add_divert_row(GTK_WINDOW(window), GTK_CONTAINER(main_box), _("No reply"), SS_DIVERT_NO_REPLY);
  add_divert_row(GTK_WINDOW(window), GTK_CONTAINER(main_box), _("Not reachable"), SS_DIVERT_NO_REACH);
  int i;
  for (i=0; i + SS_DIVERT_MIN <= SS_DIVERT_MAX; i++) {
    if (diverts[i].update_needed) {
      ss_get_divert(diverts[i].divert_type, got_divert, (void *) diverts[i].divert_type);
      break;
    }
  }

  g_signal_connect (G_OBJECT (window), "delete_event",
		    G_CALLBACK (manual_diverts_query), NULL);

  gtk_widget_show_all(GTK_WIDGET(window));
}

int main (int argc, char *argv[])
{
  DBusError dbus_error;

  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
  bind_textdomain_codeset (PACKAGE, "UTF-8");
  textdomain (PACKAGE);

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

  GtkWidget *button =  gtk_button_new_with_label ("Manual");
  gtk_container_add ( GTK_CONTAINER (main_window), button);
  g_signal_connect (button, "clicked", G_CALLBACK (show_manual_diverts), NULL);

  gtk_widget_show_all(GTK_WIDGET(main_window));
  gtk_main();
  return 0;
}
