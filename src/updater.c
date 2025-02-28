/*============================================================================
Copyright (c) 2021-2025 Raspberry Pi Holdings Ltd.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
============================================================================*/

#include <locale.h>
#include <glib/gi18n.h>

#ifdef LXPLUG
#include "plugin.h"
#else
#include "lxutils.h"
#endif

#include "updater.h"

#define I_KNOW_THE_PACKAGEKIT_GLIB2_API_IS_SUBJECT_TO_CHANGE
#include <packagekit-glib2/packagekit.h>

/*----------------------------------------------------------------------------*/
/* Typedefs and macros                                                        */
/*----------------------------------------------------------------------------*/

#define DEBUG_ON
#ifdef DEBUG_ON
#define DEBUG(fmt,args...) g_message("up: " fmt,##args)
#else
#define DEBUG(fmt,args...)
#endif

#define SECS_PER_HOUR 3600L

/*----------------------------------------------------------------------------*/
/* Global data                                                                */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static gboolean net_available (void);
static void check_for_updates (gpointer user_data);
static gpointer refresh_update_cache (gpointer data);
static void refresh_cache_done (PkTask *task, GAsyncResult *res, gpointer data);
static gboolean filter_fn (PkPackage *package, gpointer user_data);
static gboolean filter_fn_x86 (PkPackage *package, gpointer);
static void check_updates_done (PkTask *task, GAsyncResult *res, gpointer data);
static void install_updates (GtkWidget *widget, gpointer user_data);
static void launch_installer (void);
static void show_updates (GtkWidget *widget, gpointer user_data);
static void handle_close_update_dialog (GtkButton *button, gpointer user_data);
static void handle_close_and_install (GtkButton *button, gpointer user_data);
static gint delete_update_dialog (GtkWidget *widget, GdkEvent *event, gpointer user_data);
static void show_menu (UpdaterPlugin *up);
static void hide_menu (UpdaterPlugin *up);
static void update_icon (UpdaterPlugin *up, gboolean hide);
static gboolean init_check (gpointer data);
static gboolean net_check (gpointer data);
static gboolean periodic_check (gpointer data);
static void updater_button_clicked (GtkWidget *, UpdaterPlugin *up);

/*----------------------------------------------------------------------------*/
/* Function definitions                                                       */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* Utility functions                                                          */
/*----------------------------------------------------------------------------*/

static gboolean net_available (void)
{
    if (system ("hostname -I | grep -q \\\\.") == 0) return TRUE;
    else return FALSE;
}

/*----------------------------------------------------------------------------*/
/* Handlers for PackageKit asynchronous check for updates                     */
/*----------------------------------------------------------------------------*/

static void check_for_updates (gpointer user_data)
{
    UpdaterPlugin *up = (UpdaterPlugin *) user_data;

    if (!net_available ())
    {
        DEBUG ("No network connection - update check failed");
        return;
    }

    DEBUG ("Checking for updates");
    g_thread_new (NULL, refresh_update_cache, up);
}

static gpointer refresh_update_cache (gpointer data)
{
    UpdaterPlugin *up = (UpdaterPlugin *) data;
    PkTask *task = pk_task_new ();
    pk_client_refresh_cache_async (PK_CLIENT (task), TRUE, up->cancellable, NULL, NULL, (GAsyncReadyCallback) refresh_cache_done, data);
    return NULL;
}

static void refresh_cache_done (PkTask *task, GAsyncResult *res, gpointer data)
{
    UpdaterPlugin *up = (UpdaterPlugin *) data;
    GError *error = NULL;
    pk_task_generic_finish (task, res, &error);

    if (error != NULL)
    {
        DEBUG ("Error updating cache - %s", error->message);
        g_error_free (error);
        return;
    }

    DEBUG ("Cache updated - comparing versions");
    pk_client_get_updates_async (PK_CLIENT (task), PK_FILTER_ENUM_NONE, up->cancellable, NULL, NULL, (GAsyncReadyCallback) check_updates_done, data);
}

static gboolean filter_fn (PkPackage *package, gpointer)
{
    PkInfoEnum info = pk_package_get_info (package);
	switch (info)
    {
        case PK_INFO_ENUM_LOW:
        case PK_INFO_ENUM_NORMAL:
        case PK_INFO_ENUM_IMPORTANT:
        case PK_INFO_ENUM_SECURITY:
        case PK_INFO_ENUM_BUGFIX:
        case PK_INFO_ENUM_ENHANCEMENT:
        case PK_INFO_ENUM_BLOCKED:      return TRUE;
                                        break;

        default:                        return FALSE;
                                        break;
    }
}

static gboolean filter_fn_x86 (PkPackage *package, gpointer)
{
    if (strstr (pk_package_get_arch (package), "amd64")) return FALSE;
    return filter_fn (package, NULL);
}

static void check_updates_done (PkTask *task, GAsyncResult *res, gpointer data)
{
    UpdaterPlugin *up = (UpdaterPlugin *) data;
    PkPackageSack *sack = NULL, *fsack;

    GError *error = NULL;
    PkResults *results = pk_task_generic_finish (task, res, &error);

    if (error != NULL)
    {
        DEBUG ("Error comparing versions - %s", error->message);
        g_error_free (error);
        return;
    }

    sack = pk_results_get_package_sack (results);
    if (system ("raspi-config nonint is_pi"))
        fsack = pk_package_sack_filter (sack, filter_fn_x86, data);
    else
        fsack = pk_package_sack_filter (sack, filter_fn, data);

    up->n_updates = pk_package_sack_get_size (fsack);
    if (up->ids != NULL) g_strfreev (up->ids);
    if (up->n_updates > 0)
    {
        DEBUG ("Check complete - %d updates available", up->n_updates);
        up->ids = pk_package_sack_get_ids (fsack);
        lxpanel_notify (up->panel, _("Updates are available\nClick the update icon to install"));
    }
    else
    {
        DEBUG ("Check complete - no updates available");
        up->ids = NULL;
    }
    update_icon (up, FALSE);

    if (sack) g_object_unref (sack);
    g_object_unref (fsack);
}


/*----------------------------------------------------------------------------*/
/* Launch installer process                                                   */
/*----------------------------------------------------------------------------*/

static void install_updates (GtkWidget *, gpointer)
{
    launch_installer ();
}

static void launch_installer (void)
{
    char *cmd[2] = {"gui-updater", NULL};

    g_spawn_async (NULL, cmd, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
}


/*----------------------------------------------------------------------------*/
/* Dialog box showing pending updates                                         */
/*----------------------------------------------------------------------------*/

static void show_updates (GtkWidget *, gpointer user_data)
{
    UpdaterPlugin *up = (UpdaterPlugin *) user_data;
    GtkBuilder *builder;
    GtkWidget *update_list;
    GtkCellRenderer *trend = gtk_cell_renderer_text_new ();
    int count;
    char buffer[1024], *ptr, *ver;

    textdomain (GETTEXT_PACKAGE);

    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/lxplug-updater.ui");
    up->update_dlg = (GtkWidget *) gtk_builder_get_object (builder, "update_dlg");
    g_signal_connect (gtk_builder_get_object (builder, "btn_install"), "clicked", G_CALLBACK (handle_close_and_install), up);
    g_signal_connect (gtk_builder_get_object (builder, "btn_close"), "clicked", G_CALLBACK (handle_close_update_dialog), up);
    g_signal_connect (up->update_dlg, "delete_event", G_CALLBACK (delete_update_dialog), up);

    GtkListStore *ls = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
    count = 0;
    while (count < up->n_updates)
    {
        g_strlcpy (buffer, up->ids[count], sizeof (buffer));
        ptr = buffer;
        while (*ptr != ';') ptr++;
        *ptr = 0;
        ptr++;
        ver = ptr;
        while (*ptr != ';') ptr++;
        *ptr = 0;
        gtk_list_store_insert_with_values (ls, NULL, count, 0, buffer, 1, ver, -1);
        count++;
    }

    update_list = (GtkWidget *) gtk_builder_get_object (builder, "update_list");
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (update_list), -1, "Package", trend, "text", 0, NULL);
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (update_list), -1, "Version", trend, "text", 1, NULL);
    gtk_tree_view_set_model (GTK_TREE_VIEW (update_list), GTK_TREE_MODEL (ls));

    gtk_widget_show_all (up->update_dlg);
}

static void handle_close_update_dialog (GtkButton *, gpointer user_data)
{
    UpdaterPlugin *up = (UpdaterPlugin *) user_data;
    if (up->update_dlg)
    {
        gtk_widget_destroy (up->update_dlg);
        up->update_dlg = NULL;
    }
}

static void handle_close_and_install (GtkButton *, gpointer user_data)
{
    UpdaterPlugin *up = (UpdaterPlugin *) user_data;
    if (up->update_dlg)
    {
        gtk_widget_destroy (up->update_dlg);
        up->update_dlg = NULL;
    }
    launch_installer ();
}

static gint delete_update_dialog (GtkWidget *, GdkEvent *, gpointer user_data)
{
    UpdaterPlugin *up = (UpdaterPlugin *) user_data;
    handle_close_update_dialog (NULL, up);
    return TRUE;
}


/*----------------------------------------------------------------------------*/
/* Menu                                                                       */
/*----------------------------------------------------------------------------*/

static void show_menu (UpdaterPlugin *up)
{
    GtkWidget *item;

    hide_menu (up);

    up->menu = gtk_menu_new ();

    item = gtk_menu_item_new_with_label (_("Show Updates..."));
    g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (show_updates), up);
    if (up->update_dlg && gtk_widget_is_visible (up->update_dlg)) gtk_widget_set_sensitive (item, FALSE);
    gtk_menu_shell_append (GTK_MENU_SHELL (up->menu), item);

    item = gtk_menu_item_new_with_label (_("Install Updates"));
    g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (install_updates), up);
    if (up->update_dlg && gtk_widget_is_visible (up->update_dlg)) gtk_widget_set_sensitive (item, FALSE);
    gtk_menu_shell_append (GTK_MENU_SHELL (up->menu), item);

    gtk_widget_show_all (up->menu);
    wrap_show_menu (up->plugin, up->menu);
}

static void hide_menu (UpdaterPlugin *up)
{
    if (up->menu)
    {
		gtk_menu_popdown (GTK_MENU (up->menu));
		gtk_widget_destroy (up->menu);
		up->menu = NULL;
	}
}


/*----------------------------------------------------------------------------*/
/* Icon                                                                       */
/*----------------------------------------------------------------------------*/

static void update_icon (UpdaterPlugin *up, gboolean hide)
{
    /* if updates are available, show the icon */
    if (up->n_updates && !hide)
    {
        gtk_widget_show_all (up->plugin);
        gtk_widget_set_sensitive (up->plugin, TRUE);
    }
    else
    {
        gtk_widget_hide (up->plugin);
        gtk_widget_set_sensitive (up->plugin, FALSE);
    }
}


/*----------------------------------------------------------------------------*/
/* Timer handlers                                                             */
/*----------------------------------------------------------------------------*/

static gboolean init_check (gpointer data)
{
    UpdaterPlugin *up = (UpdaterPlugin *) data;
    up->idle_timer = 0;
    update_icon (up, TRUE);

    /* Don't bother with the check if the wizard is running - it checks anyway... */
    if (!system ("ps ax | grep -v grep | grep -q piwiz")) return FALSE;

    if (net_available ()) check_for_updates (up);
    else
    {
        DEBUG ("No network connection - polling...");
        up->idle_timer = g_timeout_add_seconds (60, net_check, up);
    }
    return FALSE;
}

static gboolean net_check (gpointer data)
{
    UpdaterPlugin *up = (UpdaterPlugin *) data;
    if (net_available ())
    {
        up->idle_timer = 0;
        check_for_updates (up);
        return FALSE;
    }
    DEBUG ("No network connection - polling...");
    return TRUE;
}

static gboolean periodic_check (gpointer data)
{
    UpdaterPlugin *up = (UpdaterPlugin *) data;
    check_for_updates (up);
    return TRUE;
}

/*----------------------------------------------------------------------------*/
/* wf-panel plugin functions                                                  */
/*----------------------------------------------------------------------------*/

/* Handler for button click */
static void updater_button_clicked (GtkWidget *, UpdaterPlugin *up)
{
    CHECK_LONGPRESS
    show_menu (up);
}

/* Handler for system config changed message from panel */
void updater_update_display (UpdaterPlugin *up)
{
    wrap_set_taskbar_icon (up, up->tray_icon, "update-avail");
}

/* Handler for control message */
gboolean updater_control_msg (UpdaterPlugin *up, const char *cmd)
{
    if (!strncmp (cmd, "check", 5))
    {
        update_icon (up, TRUE);
        check_for_updates (up);
        return TRUE;
    }

    return FALSE;
}

/* Handler for interval update from variable watcher */
void updater_set_interval (UpdaterPlugin *up)
{
    if (up->timer) g_source_remove (up->timer);
    if (up->interval)
        up->timer = g_timeout_add_seconds (up->interval * SECS_PER_HOUR, periodic_check, up);
    else
        up->timer = 0;
}

void updater_init (UpdaterPlugin *up)
{
    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

    /* Allocate icon as a child of top level */
    up->tray_icon = gtk_image_new ();
    gtk_container_add (GTK_CONTAINER (up->plugin), up->tray_icon);
    wrap_set_taskbar_icon (up, up->tray_icon, "update-avail");
    gtk_widget_set_tooltip_text (up->tray_icon, _("Updates are available - click to install"));

    /* Set up button */
    gtk_button_set_relief (GTK_BUTTON (up->plugin), GTK_RELIEF_NONE);
#ifndef LXPLUG
    g_signal_connect (up->plugin, "clicked", G_CALLBACK (updater_button_clicked), up);
#endif

    /* Set up variables */
    up->menu = NULL;
    up->update_dlg = NULL;
    up->n_updates = 0;
    up->ids = NULL;
    up->cancellable = g_cancellable_new ();

    /* Start timed events to monitor status */
    updater_set_interval (up);
    up->idle_timer = g_idle_add (init_check, up);

    /* Show the widget and return. */
    gtk_widget_show_all (up->plugin);
}

void updater_destructor (gpointer user_data)
{
    UpdaterPlugin *up = (UpdaterPlugin *) user_data;

    g_cancellable_cancel (up->cancellable);
    if (up->timer) g_source_remove (up->timer);
    if (up->idle_timer) g_source_remove (up->idle_timer);

    /* Deallocate memory */
    g_free (up);
}

/*----------------------------------------------------------------------------*/
/* LXPanel plugin functions                                                   */
/*----------------------------------------------------------------------------*/
#ifdef LXPLUG

/* Constructor */
static GtkWidget *updater_constructor (LXPanel *panel, config_setting_t *settings)
{
    /* Allocate and initialize plugin context */
    UpdaterPlugin *up = g_new0 (UpdaterPlugin, 1);

    /* Allocate top level widget and set into plugin widget pointer. */
    up->panel = panel;
    up->settings = settings;
    up->plugin = gtk_button_new ();
    lxpanel_plugin_set_data (up->plugin, up, updater_destructor);

    /* Read config */
    if (!config_setting_lookup_int (up->settings, "Interval", &up->interval)) up->interval = 24;

    updater_init (up);

    return up->plugin;
}

/* Handler for button press */
static gboolean updater_button_press_event (GtkWidget *plugin, GdkEventButton *event, LXPanel *)
{
    UpdaterPlugin *up = lxpanel_plugin_get_data (plugin);

    if (event->button == 1)
    {
        updater_button_clicked (plugin, up);
        return TRUE;
    }
    else return FALSE;
}

/* Handler for system config changed message from panel */
static void updater_configuration_changed (LXPanel *, GtkWidget *plugin)
{
    UpdaterPlugin *up = lxpanel_plugin_get_data (plugin);
    updater_update_display (up);
}

/* Handler for control message */
static gboolean updater_control (GtkWidget *plugin, const char *cmd)
{
    UpdaterPlugin *up = lxpanel_plugin_get_data (plugin);
    return updater_control_msg (up, cmd);
}

/* Apply changes from config dialog */
static gboolean updater_apply_configuration (gpointer user_data)
{
    UpdaterPlugin *up = lxpanel_plugin_get_data (GTK_WIDGET (user_data));

    config_group_set_int (up->settings, "Interval", up->interval);

    updater_set_interval (up);
    return FALSE;
}

/* Display configuration dialog */
static GtkWidget *updater_configure (LXPanel *panel, GtkWidget *plugin)
{
    UpdaterPlugin *up = lxpanel_plugin_get_data (plugin);

    return lxpanel_generic_config_dlg(_("Updater"), panel,
        updater_apply_configuration, plugin,
        _("Hours between checks for updates"), &up->interval, CONF_TYPE_INT,
        NULL);
}

FM_DEFINE_MODULE (lxpanel_gtk, updater)

/* Plugin descriptor */
LXPanelPluginInit fm_module_init_lxpanel_gtk = {
    .name = N_("Updater"),
    .description = N_("Checks for updates"),
    .new_instance = updater_constructor,
    .reconfigure = updater_configuration_changed,
    .button_press_event = updater_button_press_event,
    .config = updater_configure,
    .control = updater_control,
    .gettext_package = GETTEXT_PACKAGE
};
#endif

/* End of file */
/*----------------------------------------------------------------------------*/
