/* 
 * GSQL - database development tool for GNOME
 *
 * Copyright (C) 2010  Smyatkin Maxim <smyatkinmaxim@gmail.com>
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

 

#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>
#include <libgsql/common.h>
#include <libgsql/stock.h>
#include <libgsql/session.h>
#include <libgsql/navigation.h>
#include <libgsql/cvariable.h>
#include "nav_objects.h"
#include "engine_stock.h"
#include "nav_tree__arguments.h"
#include "nav_sql.h"

static GSQLNavigationItem procedures[] = {
	{   ARGUMENTS_ID,
		GSQL_STOCK_ARGUMENTS,
		N_("Input"), 
		sql_firebird_proc_in_arguments,		// sql
		NULL, 						// object_popup
		NULL,						// object_handler
		(GSQLNavigationHandler) nav_tree_refresh_arguments,	// expand_handler
		NULL,						// event_handler
		NULL, 0},
	{   ARGUMENTS_ID,
		GSQL_STOCK_ARGUMENTS,
		N_("Output"), 
		sql_firebird_proc_out_arguments,		// sql
		NULL, 						// object_popup
		NULL,						// object_handler
		(GSQLNavigationHandler) nav_tree_refresh_arguments,	// expand_handler
		NULL,						// event_handler
		NULL, 0}
};


void
nav_tree_refresh_procedures (GSQLNavigation *navigation,
						 	GtkTreeView *tv,
						 	GtkTreeIter *iter)
{
	GSQL_TRACE_FUNC;

	GtkTreeModel *model;
	GtkListStore *detail;
	GSQLNavigation *nav = NULL;
	gchar			*sql = NULL;
	gchar			*realname = NULL;
	gchar			*owner = NULL;
	gint 		id;
	gint		i,n;
	GtkTreeIter child;
	GtkTreeIter parent;
	GtkTreeIter child_fake;
	GtkTreeIter	child_last;
	GSQLCursor *cursor;
	GSQLVariable *var, *var_t, *tmp = NULL;
	GSQLSession *session;
	GSQLWorkspace *workspace;
	GSQLCursorState state;
	GtkListStore *details;
	gchar *name;
	gchar *key;
	gchar *stock = NULL;
	gchar *tbl = "%";
	gchar *parent_realname = NULL;
	
	model = gtk_tree_view_get_model(tv);
	n = gtk_tree_model_iter_n_children(model, iter);
	
	for (; n>1; n--)
	{
		gtk_tree_model_iter_children (model, &child, iter);
		gtk_tree_store_remove (GTK_TREE_STORE(model), &child);
	}
	
	gtk_tree_model_iter_children(model, &child_last, iter);
	
	gtk_tree_model_get (model, iter,  
						GSQL_NAV_TREE_REALNAME, 
						&realname, -1);
	gtk_tree_model_get (model, iter,  
						GSQL_NAV_TREE_SQL, 
						&sql, -1);
	g_return_if_fail (sql != NULL);
	
	gtk_tree_model_get (model, iter,  
						GSQL_NAV_TREE_OWNER, 
						&owner, -1);
	
	session = gsql_session_get_active ();
	
	// get parent iter. if this iter are TABLE_ID then
	// we looking for table's constraints only.
	gtk_tree_model_iter_parent (model, &parent, iter);
	
	gtk_tree_model_get (model, &parent,  
						GSQL_NAV_TREE_ID, 
						&id, -1);
	
	gtk_tree_model_get (model, &parent,  
						GSQL_NAV_TREE_REALNAME, 
						&parent_realname, -1);

	if ((id == TABLE_ID) && (parent_realname != NULL))
		tbl = parent_realname;
	GSQL_DEBUG ("realname:[%s]    sql:[%s]   owner:[%s]", realname, sql, owner);
	
	cursor = gsql_cursor_new (session, sql);
	state = gsql_cursor_open_with_bind (cursor,
										FALSE,
										GSQL_CURSOR_BIND_BY_POS,
										G_TYPE_STRING, tbl,
										-1);
	
	var = g_list_nth_data(cursor->var_list,0);
	var_t = g_list_nth_data(cursor->var_list,1);
	
	if (state != GSQL_CURSOR_STATE_OPEN)
	{
		gsql_cursor_close (cursor);
		return;		
	}
	
	i = 0;
	
	while (gsql_cursor_fetch (cursor, 1) > 0)
	{
		i++;
		
		if (var->value_type != G_TYPE_STRING)
		{
			GSQL_DEBUG ("The name of object should be a string (char *). Is the bug");
			name = N_("Incorrect data");
		} else {
			if (var->raw_to_value)
			{
				tmp = g_malloc0(sizeof(GSQLVariable));
				memcpy(tmp, var, sizeof(GSQLVariable));
				var->raw_to_value (tmp);
			}
			name = (gchar *) var->value;
			// make a key for a hash of details
			guint length = 0;
			length = g_snprintf (NULL,0, "%x%s%d%s",
				   session, (gchar *) tmp->value, PROCEDURE_ID, (gchar *) tmp->value);
			key = g_malloc0(length);
			memset (key, 0, length);
			g_snprintf (key, length - 1, "%x%s%d%s",
				   session, (gchar *) tmp->value, PROCEDURE_ID, (gchar *) tmp->value);
			
			details = gsql_navigation_get_details (navigation, key);
			firebird_navigation_fill_details (cursor, details);
		}
		
		gtk_tree_store_append (GTK_TREE_STORE(model), &child, iter);
		gtk_tree_store_set (GTK_TREE_STORE(model), &child,
					GSQL_NAV_TREE_ID,			PROCEDURE_ID,
					GSQL_NAV_TREE_OWNER,		owner,
					GSQL_NAV_TREE_IMAGE,		GSQL_STOCK_PROCEDURES,
					GSQL_NAV_TREE_NAME,			name,
					GSQL_NAV_TREE_REALNAME, 	(gchar *) tmp->value,
					GSQL_NAV_TREE_ITEM_INFO, 	NULL,
					GSQL_NAV_TREE_SQL,			NULL,
					GSQL_NAV_TREE_OBJECT_POPUP, NULL,
					GSQL_NAV_TREE_OBJECT_HANDLER, NULL,
					GSQL_NAV_TREE_EXPAND_HANDLER, NULL,
					GSQL_NAV_TREE_EVENT_HANDLER, NULL,
					GSQL_NAV_TREE_STRUCT, procedures,
					GSQL_NAV_TREE_DETAILS, details,
					GSQL_NAV_TREE_NUM_ITEMS, G_N_ELEMENTS(procedures),
					-1);
	
		gtk_tree_store_append (GTK_TREE_STORE (model), &child_fake, &child);
		gtk_tree_store_set (GTK_TREE_STORE (model), &child_fake,
				GSQL_NAV_TREE_ID,				-1,
				GSQL_NAV_TREE_IMAGE,			NULL,
				GSQL_NAV_TREE_NAME,				N_("Processing..."),
				GSQL_NAV_TREE_REALNAME,			NULL,
				GSQL_NAV_TREE_ITEM_INFO,		NULL,
				GSQL_NAV_TREE_SQL,				NULL,
				GSQL_NAV_TREE_OBJECT_POPUP,		NULL,
				GSQL_NAV_TREE_OBJECT_HANDLER,	NULL,
				GSQL_NAV_TREE_EXPAND_HANDLER,	NULL,
				GSQL_NAV_TREE_EVENT_HANDLER,	NULL,
				GSQL_NAV_TREE_STRUCT,			NULL,
				GSQL_NAV_TREE_NUM_ITEMS, 		NULL,
				-1);
		g_free (key);
		if (tmp) g_free(tmp);
	}
	GSQL_DEBUG ("Items fetched: [%d]", i);
	
	if (i > 0)
	{
		name = g_strdup_printf("%s<span weight='bold'> [%d]</span>", 
												realname, i);
		gtk_tree_store_set (GTK_TREE_STORE(model), iter,
							GSQL_NAV_TREE_NAME, 
							name,
							-1);
		g_free (name);
	}
	
	gtk_tree_store_remove(GTK_TREE_STORE(model), &child_last);
	
	gsql_cursor_close (cursor);

}
