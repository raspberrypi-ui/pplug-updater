gtk = dependency('gtk+-3.0')
gtkmm = dependency('gtkmm-3.0', version: '>=3.24')
lxpanel = dependency('lxpanel-pi')
wfpanel = dependency('wf-panel-pi')
packagekit = dependency('packagekit-glib2')

lsources = files(
  'updater.c'
)

ldeps = [ gtk, lxpanel, packagekit ]

largs = [ '-DPACKAGE_DATA_DIR="' + lresource_dir + '"', '-DGETTEXT_PACKAGE="lxplug_' + meson.project_name() + '"' ]

shared_module(meson.project_name(), lsources,
        dependencies: ldeps,
        install: true,
        install_dir: get_option('libdir') / 'lxpanel-pi',
        c_args : largs,
        name_prefix: ''
)

wsources = files(
  'updater.cpp'
)

wdeps = [ gtkmm, wfpanel, packagekit ]

wargs = [ '-DPACKAGE_DATA_DIR="' + wresource_dir + '"', '-DGETTEXT_PACKAGE="wfplug_' + meson.project_name() +'"' ]

shared_module('lib' + meson.project_name(), [ lsources, wsources ],
        dependencies: wdeps,
        install: true,
        install_dir: get_option('libdir') / 'wf-panel-pi',
        c_args : wargs,
        cpp_args : wargs,
        name_prefix: ''
)

metadata = files(
  'updater.xml'
)
install_data(metadata, install_dir: metadata_dir)
