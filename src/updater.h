/*============================================================================
Copyright (c) 2021-2025 Raspberry Pi
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

/*----------------------------------------------------------------------------*/
/* Typedefs and macros                                                        */
/*----------------------------------------------------------------------------*/

typedef struct 
{
    GtkWidget *plugin;

#ifdef LXPLUG
    LXPanel *panel;                 /* Back pointer to panel */
    config_setting_t *settings;     /* Plugin settings */
#else
    int icon_size;                  /* Variables used under wf-panel */
    gboolean bottom;
#endif

    GtkWidget *tray_icon;           /* Displayed image */
    GtkWidget *menu;                /* Popup menu */
    GtkWidget *update_dlg;          /* Widget used to display pending update list */
    int n_updates;                  /* Number of pending updates */
    gchar **ids;                    /* ID strings for pending updates */
    int interval;                   /* Number of hours between periodic checks */
    guint timer;                    /* Periodic check timer ID */
    guint idle_timer;
    GCancellable *cancellable;
} UpdaterPlugin;

/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

extern void updater_init (UpdaterPlugin *up);
extern void updater_update_display (UpdaterPlugin *up);
extern void updater_set_interval (UpdaterPlugin *up);
extern gboolean updater_control_msg (UpdaterPlugin *up, const char *cmd);
extern void updater_destructor (gpointer user_data);

/* End of file */
/*----------------------------------------------------------------------------*/
