/*
 *  Copyright © 2005, 2006 Jean-François Rameau
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

#include "ephy-net-monitor.h"

#include "ephy-dbus.h"
#include "ephy-debug.h"
#include "ephy-settings.h"
#include "ephy-prefs.h"

#include <NetworkManager.h>

#include <gmodule.h>

typedef enum 
{
	NETWORK_UP,
	NETWORK_DOWN
} NetworkStatus;

#define EPHY_NET_MONITOR_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_NET_MONITOR, EphyNetMonitorPrivate))

struct _EphyNetMonitorPrivate
{
	DBusConnection *bus;
	guint active : 1;
	NetworkStatus status;
};

enum
{
	PROP_0,
	PROP_NETWORK_STATUS
};

G_DEFINE_TYPE (EphyNetMonitor, ephy_net_monitor, G_TYPE_OBJECT)

static void
ephy_net_monitor_set_net_status (EphyNetMonitor *monitor,
				 NetworkStatus status)
{
	EphyNetMonitorPrivate *priv = monitor->priv;

	LOG ("EphyNetMonitor turning Epiphany to %s mode",
	     status != NETWORK_DOWN ? "online" : "offline");

	priv->status = status;

	g_object_notify (G_OBJECT (monitor), "network-status");
}

static void 
ephy_net_monitor_dbus_notify (DBusPendingCall *pending,
                    	      EphyNetMonitor *monitor) 
{
	DBusMessage* msg = dbus_pending_call_steal_reply (pending);
	
	LOG ("EphyNetMonitor getting response from dbus");

	if (!msg) return;

	if (dbus_message_get_type(msg) == DBUS_MESSAGE_TYPE_METHOD_RETURN) 
	{
		dbus_uint32_t result;

		if (dbus_message_get_args (msg, NULL, DBUS_TYPE_UINT32, &result,
					  DBUS_TYPE_INVALID)) 
		{
			NetworkStatus net_status;

			net_status = result == NM_STATE_CONNECTED ? NETWORK_UP : NETWORK_DOWN;

			LOG ("EphyNetMonitor guesses the network is %s", 
					net_status != NETWORK_DOWN ? "up" : "down");

			ephy_net_monitor_set_net_status (monitor, net_status);
		}
		dbus_message_unref (msg);
	}
}

/* This is the heart of Net Monitor monitor */
/* ATM, if there is an active device, we say that network is up: that's all ! */
static void
ephy_net_monitor_check_network (EphyNetMonitor *monitor)
{
	EphyNetMonitorPrivate *priv = monitor->priv;
	DBusMessage *message;
	DBusPendingCall* reply;

	g_return_if_fail (priv != NULL && priv->bus != NULL);

	LOG ("EphyNetMonitor checking network");

	/* ask to Network Manager if there is at least one active device */
	message = dbus_message_new_method_call (NM_DBUS_SERVICE, 
						NM_DBUS_PATH, 
						NM_DBUS_INTERFACE, 
						"state");

	if (message == NULL)
	{
		g_warning ("Couldn't allocate the dbus message");
		/* fallback: let's Epiphany roll */
		return;
	}

	if (dbus_connection_send_with_reply (priv->bus, message, &reply, -1)) 
	{
		dbus_pending_call_set_notify (reply,
					      (DBusPendingCallNotifyFunction)ephy_net_monitor_dbus_notify,
					      monitor, NULL);
		dbus_pending_call_unref (reply);
	}
	
	dbus_message_unref (message);
}

/* Filters all the messages from Network Manager */
static DBusHandlerResult
filter_func (DBusConnection *connection,
	     DBusMessage *message,
	     void *user_data)
{
	EphyNetMonitor *monitor;

	g_return_val_if_fail (user_data != NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	monitor = EPHY_NET_MONITOR (user_data);

	if (dbus_message_is_signal (message,
				    NM_DBUS_INTERFACE,
				    "StateChange"))
	{
		LOG ("EphyNetMonitor catches StateChange signal");

		ephy_net_monitor_check_network (monitor);

		return DBUS_HANDLER_RESULT_HANDLED;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void
ephy_net_monitor_attach_to_dbus (EphyNetMonitor *monitor)
{
	EphyNetMonitorPrivate *priv = monitor->priv;
	DBusError error;
	EphyDbus *dbus;
	DBusGConnection *g_connection;
	
	LOG ("EphyNetMonitor is trying to attach to SYSTEM bus");

	dbus = ephy_dbus_get_default ();

	g_connection = ephy_dbus_get_bus (dbus, EPHY_DBUS_SYSTEM);
	if (g_connection == NULL) return;
	
	priv->bus = dbus_g_connection_get_connection (g_connection);

	if (priv->bus != NULL)
	{
		dbus_connection_add_filter (priv->bus, 
					    filter_func, 
					    monitor,
					    NULL);
		dbus_error_init (&error);
		dbus_bus_add_match (priv->bus, 
				    "type='signal',interface='" NM_DBUS_INTERFACE "'", 
				    &error);
		if (dbus_error_is_set(&error)) 
		{
			g_warning("EphyNetMonitor cannot register signal handler: %s: %s", 
				  error.name, error.message);
			dbus_error_free(&error);
		}
		LOG ("EphyNetMonitor attached to SYSTEM bus");
	}
}

static void
ephy_net_monitor_detach_from_dbus (EphyNetMonitor *monitor)
{
	EphyNetMonitorPrivate *priv = monitor->priv;
	
	LOG ("EphyNetMonitor is trying to detach from SYSTEM bus");

	if (priv->bus != NULL)
	{
		dbus_connection_remove_filter (priv->bus, 
					       filter_func, 
					       monitor);
		priv->bus = NULL;
	}
}


static void
connect_to_system_bus_cb (EphyDbus *dbus,
			  EphyDbusBus kind,
			  EphyNetMonitor *monitor)
{
	if (kind == EPHY_DBUS_SYSTEM)
	{
		LOG ("EphyNetMonitor connecting to SYSTEM bus");

		ephy_net_monitor_attach_to_dbus (monitor);
	}
}

static void
disconnect_from_system_bus_cb (EphyDbus *dbus,
			       EphyDbusBus kind,
			       EphyNetMonitor *monitor)
{
	if (kind == EPHY_DBUS_SYSTEM)
	{
		LOG ("EphyNetMonitor disconnected from SYSTEM bus");

		/* no bus anymore */
		ephy_net_monitor_detach_from_dbus (monitor);
	}
}

static void
ephy_net_monitor_startup (EphyNetMonitor *monitor)
{
	EphyDbus *dbus;

	dbus = ephy_dbus_get_default ();

	LOG ("EphyNetMonitor starting up");

	ephy_net_monitor_attach_to_dbus (monitor);

	if (monitor->priv->bus != NULL)
       	{
		/* DBUS may disconnect us at any time. So listen carefully to it */
		g_signal_connect (dbus, "connected",  
				  G_CALLBACK (connect_to_system_bus_cb), monitor);
		g_signal_connect (dbus, "disconnected",  
				  G_CALLBACK (disconnect_from_system_bus_cb), monitor);

		ephy_net_monitor_check_network (monitor);
	}
}

static void
ephy_net_monitor_shutdown (EphyNetMonitor *monitor)
{
	EphyNetMonitorPrivate *priv = monitor->priv;
	EphyDbus *dbus;

	dbus = ephy_dbus_get_default ();
			
	ephy_net_monitor_detach_from_dbus (monitor);
	g_signal_handlers_disconnect_by_func
		(dbus, G_CALLBACK (connect_to_system_bus_cb), monitor);
	g_signal_handlers_disconnect_by_func
		(dbus,G_CALLBACK (disconnect_from_system_bus_cb), monitor);
		
	priv->bus = NULL;

	LOG ("EphyNetMonitor shutdown");
}

static void
ephy_net_monitor_init (EphyNetMonitor *monitor)
{
	EphyNetMonitorPrivate *priv;

	priv = monitor->priv = EPHY_NET_MONITOR_GET_PRIVATE (monitor);

	LOG ("EphyNetMonitor initialising");

	priv->status = NETWORK_UP;

	ephy_net_monitor_startup (monitor);
}

static void
ephy_net_monitor_dispose (GObject *object)
{
	EphyNetMonitor *monitor = EPHY_NET_MONITOR (object);

	LOG ("EphyNetMonitor finalising");

	ephy_net_monitor_shutdown (monitor);

	G_OBJECT_CLASS (ephy_net_monitor_parent_class)->dispose (object);
}

static void
ephy_net_monitor_get_property (GObject *object,
			       guint prop_id,
			       GValue *value,
			       GParamSpec *pspec)
{
	EphyNetMonitor *monitor = EPHY_NET_MONITOR (object);

	switch (prop_id)
	{
		case PROP_NETWORK_STATUS:
			g_value_set_boolean (value, ephy_net_monitor_get_net_status (monitor));
			break;
	}
}

static void
ephy_net_monitor_class_init (EphyNetMonitorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = ephy_net_monitor_dispose;
	object_class->get_property = ephy_net_monitor_get_property;

/**
 * EphyNetMonitor::network-status:
 * 
 * Whether the network is on-line.
 */
	g_object_class_install_property
		(object_class,
		 PROP_NETWORK_STATUS,
		 g_param_spec_boolean ("network-status",
				       "network-status",
				       "network-status",
				       FALSE,
				       G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	g_type_class_add_private (object_class, sizeof (EphyNetMonitorPrivate));
}

/* public API */

EphyNetMonitor *
ephy_net_monitor_new (void)
{
	return g_object_new (EPHY_TYPE_NET_MONITOR, NULL);
}

gboolean
ephy_net_monitor_get_net_status	(EphyNetMonitor *monitor)
{
	EphyNetMonitorPrivate *priv;
	gboolean managed;

	g_return_val_if_fail (EPHY_IS_NET_MONITOR (monitor), FALSE);

	priv = monitor->priv;
	managed = g_settings_get_boolean (EPHY_SETTINGS_MAIN,
					  EPHY_PREFS_MANAGED_NETWORK);

	return !managed || priv->status != NETWORK_DOWN;
}