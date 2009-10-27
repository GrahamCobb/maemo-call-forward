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

#include <hildon/hildon.h>

int main (int argc, char *argv[])
{
  hildon_gtk_init(&argc, &argv);

  HildonProgram *program = hildon_program_get_instance ();
  HildonStackableWindow *main_window = HILDON_STACKABLE_WINDOW (hildon_stackable_window_new());
  hildon_program_add_window (program, HILDON_WINDOW (main_window));
  g_signal_connect (G_OBJECT (main_window), "delete_event",
                    G_CALLBACK (gtk_main_quit), NULL);
  
  gtk_widget_show_all(GTK_WIDGET(main_window));
  gtk_main();
  return 0;
}
