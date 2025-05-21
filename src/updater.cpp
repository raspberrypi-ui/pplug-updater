/*============================================================================
Copyright (c) 2024 Raspberry Pi
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

#include <glibmm.h>
#include "gtk-utils.hpp"
#include "updater.hpp"

extern "C" {
    WayfireWidget *create () { return new WayfireUpdater; }
    void destroy (WayfireWidget *w) { delete w; }

    const conf_table_t *config_params (void) { return conf_table; };
    const char *display_name (void) { return N_(PLUGIN_TITLE); };
    const char *package_name (void) { return GETTEXT_PACKAGE; };
}

void WayfireUpdater::icon_size_changed_cb (void)
{
    up->icon_size = icon_size;
    updater_update_display (up);
}

void WayfireUpdater::command (const char *cmd)
{
    updater_control_msg (up, cmd);
}

bool WayfireUpdater::set_icon (void)
{
    updater_update_display (up);
    return false;
}

void WayfireUpdater::settings_changed_cb (void)
{
    up->interval = interval;
    updater_set_interval (up);
}

void WayfireUpdater::init (Gtk::HBox *container)
{
    /* Create the button */
    plugin = std::make_unique <Gtk::Button> ();
    plugin->set_name (PLUGIN_NAME);
    container->pack_start (*plugin, false, false);

    /* Setup structure */
    up = g_new0 (UpdaterPlugin, 1);
    up->plugin = (GtkWidget *)((*plugin).gobj());
    up->icon_size = icon_size;
    icon_timer = Glib::signal_idle().connect (sigc::mem_fun (*this, &WayfireUpdater::set_icon));

    /* Add long press for right click */
    gesture = add_longpress_default (*plugin);

    /* Initialise the plugin */
    updater_init (up);

    /* Setup callbacks */
    icon_size.set_callback (sigc::mem_fun (*this, &WayfireUpdater::icon_size_changed_cb));

    interval.set_callback (sigc::mem_fun (*this, &WayfireUpdater::settings_changed_cb));
}

WayfireUpdater::~WayfireUpdater()
{
    icon_timer.disconnect ();
    updater_destructor (up);
}

/* End of file */
/*----------------------------------------------------------------------------*/
