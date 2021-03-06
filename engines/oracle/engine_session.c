/* 
 * GSQL - database development tool for GNOME
 *
 * Copyright (C) 2006-2008  Taras Halturin  halturin@gmail.com
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */


#include <libgsql/engines.h>
#include <libgsql/common.h>
#include <libgsql/editor.h>
#include <libgsql/cursor.h>
#include <libgsql/cvariable.h>
#include <libgsql/menu.h>
#include <string.h>
#include "engine_session.h"
#include "engine_menu.h"
#include "nav_tree__schemas.h"


static void on_session_close (GSQLSession *session, gpointer user_data);
static void on_session_reopen (GSQLSession *session, gpointer user_data);
static void on_session_duplicate (GSQLSession *session, gpointer user_data);
static void on_session_rollback (GSQLSession *session, gpointer user_data);
static void on_session_commit (GSQLSession *session, gpointer user_data);
static void on_session_switch (GSQLSession *session, gpointer user_data);

gpointer
engine_session_open (GtkWidget *logon_widget, gchar *buffer)
{
	GSQL_TRACE_FUNC;

	GtkWidget *widget;
		
	const gchar *username,
				*password,
				*database;
	gchar info[32];
	
	GSQLSession  *session;
	GSQLSession  *current;
	GSQLEngine   *engine;
	GSQLCursor   *cursor;
	GSQLCursorState c_state;
	GSQLWorkspace *workspace;
	GSQLNavigation *navigation;
	GSQLEOracleSession *spec;
	GList   *lst;
	gchar *sql = "select version from product_component_version "
	 			 "where instr(lower(product),'oracle') >0  "
				 "and rownum <2 ";
	GSQLVariable *variable;
	ub4 mode;

	
	gint connect_as = 0;
	widget = g_object_get_data (G_OBJECT (logon_widget), "username");
	username = gtk_entry_get_text (GTK_ENTRY (widget));
	widget = g_object_get_data (G_OBJECT (logon_widget), "password");
	password = gtk_entry_get_text (GTK_ENTRY (widget));
	widget = g_object_get_data (G_OBJECT (logon_widget), "database");
	database = gtk_combo_box_get_active_text (GTK_COMBO_BOX (widget));
	widget = g_object_get_data (G_OBJECT (logon_widget), "mode");
	
	switch (gtk_combo_box_get_active (GTK_COMBO_BOX (widget)))
	{
		case 2:
			GSQL_DEBUG ("Session mode: OCI_SYSOPER");
			mode = OCI_SYSOPER;
			break;
		case 1:
			GSQL_DEBUG ("Session mode: OCI_SYSDBA");
			mode = OCI_SYSDBA;
			break;
			
		case 0:
		default:
			GSQL_DEBUG ("Session mode: OCI_DEFAULT");
			mode = OCI_DEFAULT;
	}
	
	
	
	spec = g_malloc0 (sizeof (GSQLEOracleSession));
	
	
	spec->mode = mode;
	
	if (!oracle_session_open (spec, username, password, database, buffer)) {
		
		g_free (spec);
		
		return NULL;
	}
	
	session = gsql_session_new_with_attrs ("session-username", 
										   g_strdup(username),
										   "session-password",
										   g_strdup (password),
										   "session-database",
										   g_strdup (database),
										   NULL);
	session->spec = spec;
	
	GSQL_FIXME;
	/* <workaround> */
	engine = g_object_get_data (G_OBJECT (logon_widget), "engine");
	session->engine = engine;
	/* </workaround> */
	
	cursor = gsql_cursor_new (session, sql);
	c_state = gsql_cursor_open (cursor, FALSE); 
	
	memset ((void *) info, 0, 32);
	
	if ((c_state == GSQL_CURSOR_STATE_OPEN) && (gsql_cursor_fetch (cursor, 1)))
	{
		lst = g_list_first (cursor->var_list);
		variable = GSQL_VARIABLE (lst->data);
		g_snprintf (info, 32, "%s", (gchar *) variable->value);
		
	} else {
		g_snprintf (info, 32, "%s", "0.0.0.0");
	}
	
	gsql_cursor_close (cursor);
	
	gsql_session_set_attrs (session, "session-info",
							info,
							NULL);	 
	
	workspace = gsql_workspace_new (session);
	navigation = gsql_workspace_get_navigation (workspace);
	
	nav_tree_set_root (navigation, (gchar *) username);
	
	
	
	g_signal_connect (G_OBJECT (session), "close",
					  G_CALLBACK (on_session_close), session);
	g_signal_connect (G_OBJECT (session), "reopen",
					  G_CALLBACK (on_session_reopen), session);
	g_signal_connect (G_OBJECT (session), "duplicate",
					  G_CALLBACK (on_session_duplicate), session);
	g_signal_connect (G_OBJECT (session), "commit",
					  G_CALLBACK (on_session_commit), session);
	g_signal_connect (G_OBJECT (session), "rollback",
					  G_CALLBACK (on_session_rollback), session);
	g_signal_connect (G_OBJECT (session), "switch",
					  G_CALLBACK (on_session_switch), session);
	
	g_snprintf(buffer, 256,
			   _("Connect to the Oracle database \"<b>%s</b>\" succesfully\n"
				 "<small>(%s)</small>"), 
			   g_utf8_strup (database, g_utf8_strlen (database, 128)),
			   spec->server_version);
	
	gsql_message_add (workspace, GSQL_MESSAGE_NORMAL, buffer);
	
	GSQL_DEBUG ("New session created with name [%s]", gsql_session_get_name (session));
	
	return session;
}

/* Static section:
 *
 *
 *
 *
 *
 */

static void
on_session_close (GSQLSession *session, gpointer user_data)
{
	GSQL_TRACE_FUNC;
	
	oracle_session_close (session, NULL);
}

static void
on_session_reopen (GSQLSession *session, gpointer user_data)
{
	GSQL_TRACE_FUNC;

	oracle_session_reopen (session);
}

static void
on_session_duplicate (GSQLSession *session, gpointer user_data)
{
	GSQL_TRACE_FUNC;
	
	GSQLSession *new_session;
	GSQLEOracleSession *new_spec, *spec;
	gchar *username, *database, *password;
	GSQLWorkspace *new_workspace, *workspace;
	GSQLNavigation *navigation;
	gchar buffer[256], info[32];
	GSQLCursor *cursor;
	GSQLVariable *variable;
	GSQLCursorState c_state;
	GtkWidget   *sessions;
	gchar *session_name;
	GList *lst;
	GtkWidget	*header;
	gint ret;
	gchar *sql = "select version from product_component_version "
	 			 "where instr(lower(product),'oracle') >0  "
				 "and rownum <2 ";
	
	GSQL_FIXME;
	/* Do rework this function, not yet, later. It seems like a hack :) */
	
	spec = session->spec;
	
	new_spec = g_malloc0 (sizeof (GSQLEOracleSession));
	new_spec->mode = spec->mode;
	
	username = (gchar *) gsql_session_get_username (session);
	password = (gchar *) gsql_session_get_password (session);
	database = (gchar *) gsql_session_get_database_name (session);

	workspace = gsql_session_get_workspace (session);
	
	if (!oracle_session_open (new_spec, username, password, database, buffer)) 
	{	
		g_free (spec);
		gsql_message_add (workspace, GSQL_MESSAGE_ERROR, buffer);
		
		return;
	}
	
	new_session = gsql_session_new_with_attrs ("session-username", 
										   username,
										   "session-password",
										   password,
										   "session-database",
										   database,
										   NULL);
	new_session->spec = new_spec;	
	new_session->engine = session->engine;

	cursor = gsql_cursor_new (session, sql);
	c_state = gsql_cursor_open (cursor, FALSE); 
	
	memset ((void *) info, 0, 32);
	
	if ((c_state == GSQL_CURSOR_STATE_OPEN) && (gsql_cursor_fetch (cursor, 1)))
	{
		lst = g_list_first (cursor->var_list);
		variable = GSQL_VARIABLE (lst->data);
		g_snprintf (info, 32, "%s", (gchar *) variable->value);
		
	} else {
		g_snprintf (info, 32, "%s", "0.0.0.0");
	}
	
	gsql_cursor_close (cursor);
	
	gsql_session_set_attrs (new_session, "session-info",
							info,
							NULL);	 
	
	new_workspace = gsql_workspace_new (new_session);
	navigation = gsql_workspace_get_navigation (new_workspace);
	
	nav_tree_set_root (navigation, (gchar *) username);
	
	
	
	g_signal_connect (G_OBJECT (new_session), "close",
					  G_CALLBACK (on_session_close), new_session);
	g_signal_connect (G_OBJECT (new_session), "reopen",
					  G_CALLBACK (on_session_reopen), new_session);
	g_signal_connect (G_OBJECT (new_session), "duplicate",
					  G_CALLBACK (on_session_duplicate), new_session);
	g_signal_connect (G_OBJECT (new_session), "commit",
					  G_CALLBACK (on_session_commit), new_session);
	g_signal_connect (G_OBJECT (new_session), "rollback",
					  G_CALLBACK (on_session_rollback), new_session);
	g_signal_connect (G_OBJECT (new_session), "switch",
					  G_CALLBACK (on_session_switch), new_session);
	
	g_snprintf(buffer, 256,
			   _("Connect to the Oracle database \"<b>%s</b>\" succesfully\n"
				 "<small>(%s)</small>"), 
			   g_utf8_strup (database, g_utf8_strlen (database, 128)),
			   new_spec->server_version);
	
	gsql_message_add (new_workspace, GSQL_MESSAGE_NORMAL, buffer);
	
	GSQL_DEBUG ("New session created with name [%s]", gsql_session_get_name (new_session));
	
	sessions = g_object_get_data(G_OBJECT(gsql_window), "sessions");
	
	session_name = gsql_session_get_name (new_session);
	header = gsql_utils_header_new (create_pixmap(new_session->engine->file_logo),
									   session_name, NULL,
									   FALSE, (gint) 1);
	
	gtk_widget_show (GTK_WIDGET (new_session));
	
	ret = gtk_notebook_append_page (GTK_NOTEBOOK(sessions),
							  GTK_WIDGET (new_session), 
							  header);

	gtk_notebook_set_current_page (GTK_NOTEBOOK(sessions), ret);
	gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK(sessions),
							  GTK_WIDGET (new_session), TRUE);
	
}

static void
on_session_commit (GSQLSession *session, gpointer user_data)
{
	GSQL_TRACE_FUNC;;
	
	GSQLWorkspace *workspace;
	
	g_return_if_fail (GSQL_IS_SESSION (session));
	
	oracle_session_commit (session);
}

static void
on_session_rollback (GSQLSession *session, gpointer user_data)
{
	GSQL_TRACE_FUNC;
	
	GSQLWorkspace *workspace;
	
	g_return_if_fail (GSQL_IS_SESSION (session));
	
	oracle_session_rollback (session);
}


static void
on_session_switch (GSQLSession *session, gpointer user_data)
{
	GSQL_TRACE_FUNC;
	
	GSQLSession *current;
	GSQLEOracleSession *spec_session;
	GtkWidget *widget;
	GtkAction *act;
	
	g_return_if_fail (GSQL_IS_SESSION (session));
	
	current = gsql_session_get_active ();
	
	if (current == session)
	{
		gsql_engine_menu_set_status (session->engine, TRUE);
		
		spec_session = session->spec;
		
		widget = gsql_menu_get_widget ("/MenuMain/PHolderEngines/MenuOracle/OracleServerOutput");

		act = gtk_action_group_get_action (session->engine->action, "OracleActionServerOutput");
		gtk_action_block_activate_from (act, widget);
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (widget), spec_session->dbms_output);
		gtk_action_unblock_activate_from (act, widget);

		GSQL_DEBUG ("Oracle engine. Yes, It is mine");
		
	} else {
		
		gsql_engine_menu_set_status (session->engine, FALSE);

			GSQL_DEBUG ("Oracle engine. No, It is not mine");
	}
	
}
