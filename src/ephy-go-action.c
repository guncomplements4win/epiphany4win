/*
 *  Copyright © 2003 Marco Pesenti Gritti
 *  Copyright © 2003 David Bordoley
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#include "ephy-go-action.h"
#include "ephy-debug.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

static void ephy_go_action_class_init (EphyGoActionClass *class);

G_DEFINE_TYPE (EphyGoAction, ephy_go_action, EPHY_TYPE_LINK_ACTION)

static GtkWidget *
create_tool_item (GtkAction *action)
{
	GtkWidget *button;
	GtkWidget *item;

	item = GTK_WIDGET (gtk_tool_item_new ());

	button = gtk_button_new_with_label (_("Go"));
	gtk_button_set_relief(GTK_BUTTON (button), GTK_RELIEF_NONE);
	gtk_button_set_focus_on_click (GTK_BUTTON (button), FALSE);

	gtk_container_add (GTK_CONTAINER (item), button);
	gtk_widget_show (button);

	return item;
}

static gboolean
button_clicked_cb (GtkWidget *widget,
		   GdkEventButton *event,
		   gpointer user_data)
{
	if (event->button == 1 || event->button == 2)
		gtk_action_activate (GTK_ACTION (user_data));
	
	return FALSE;
}

static void
connect_proxy (GtkAction *action,
	       GtkWidget *proxy)
{      
	GTK_ACTION_CLASS (ephy_go_action_parent_class)->connect_proxy (action, proxy);

	if (GTK_IS_TOOL_ITEM (proxy))
	{
		g_signal_connect (gtk_bin_get_child (GTK_BIN (proxy)), "button-release-event",
				  G_CALLBACK (button_clicked_cb), action);
	}
}

static void
disconnect_proxy (GtkAction *action,
		  GtkWidget *proxy)
{
	g_signal_handlers_disconnect_by_func
		(proxy, G_CALLBACK (gtk_action_activate), action);

	GTK_ACTION_CLASS (ephy_go_action_parent_class)->disconnect_proxy (action, proxy);
}

static void
ephy_go_action_class_init (EphyGoActionClass *class)
{
	GtkActionClass *action_class = GTK_ACTION_CLASS (class);

	action_class->create_tool_item = create_tool_item;
	action_class->connect_proxy = connect_proxy;
	action_class->disconnect_proxy = disconnect_proxy;
}

static void
ephy_go_action_init (EphyGoAction *action)
{
        /* Empty, needed for G_DEFINE_TYPE macro */
}
