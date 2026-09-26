qt6 = import('qt6')

conf_dir = meson.project_source_root() / 'conf'
rsrc_dir = meson.project_source_root() / 'rsrc'

# Render the bundled markdown docs to HTML (About dialog, plugin help, ...).
# changelog now lives at the repo root as CHANGELOG.md, shared with the
# AppStream <releases> generation below; the rest are still under docs/.
cmark = find_program('cmark')
doc_html = []
doc_sources = {
    'changelog': meson.project_source_root() / 'CHANGELOG.md',
    'acknowledgments': meson.project_source_root() / 'docs' / 'acknowledgments.md',
    'privacy': meson.project_source_root() / 'docs' / 'privacy.md',
    'plugin': meson.project_source_root() / 'docs' / 'plugin.md',
}
foreach doc, doc_source : doc_sources
    doc_html += custom_target(
        doc + '.html',
        input: doc_source,
        output: doc + '.html',
        command: [cmark, '@INPUT@'],
        capture: true,
        build_by_default: true,
        install: true,
        install_dir: resources_dir,
    )
endforeach

# Compiled Qt resource bundle. No name: so rcc keeps the qrc basename and emits
# qInitResources_resources(), which Application.cpp calls via Q_INIT_RESOURCE.
app_resources = qt6.compile_resources(
    sources: rsrc_dir / 'resources.qrc',
)

# DBusPawmmit in Application.h is behind #ifdef Q_OS_LINUX; moc needs the macro
# to emit its meta-object.
app_moc = qt6.preprocess(
    moc_headers: files('Application.h'),
    moc_extra_arguments: host_sys == 'linux' ? ['-DQ_OS_LINUX'] : [],
    dependencies: [qt6_dep, pawmmit_common],
)

app_public_args = [
    '-DPAWMMIT_NAME="Pawmmit"',
    '-DPAWMMIT_VERSION="' + meson.project_version() + '"',
    '-DPAWMMIT_IDENTIFIER="com.github.Pawmmit.Pawmmit"',
    '-DPAWMMIT_ORGANIZATION_DOMAIN="Pawmmit.github.com"',
    '-DPAWMMIT_DBUS_INTERFACE_NAME="com.github.Pawmmit.Pawmmit.Application"',
    '-DPAWMMIT_DBUS_OBJECT_PATH="/com/github/Pawmmit/Pawmmit/Application"',
]
app_private_args = [
    '-DQT_TRANSLATIONS_DIR="' + qt_translations_dir + '"',
    '-DBUILD_DESCRIPTION="' + build_description + '"',
]

app_src = [files('Application.cpp', 'CustomTheme.cpp', 'Theme.cpp')]
app_deps = [
    pawmmit_common,
    translation_dep,
    conf_dep,
    git_dep,
    ui_dep,
    editor_dep,
    update_dep,
]

if is_mac
    app_src += files('Application_mac.mm', 'CustomTheme_mac.mm')
    app_deps += dependency('appleframeworks', modules: ['AppKit', 'Foundation', 'Cocoa'])
elif not is_win
    app_deps += dependency('qt6', modules: ['DBus'])
endif

app_lib = static_library(
    'app',
    app_src + app_moc + app_resources,
    cpp_args: app_public_args + app_private_args,
    objcpp_args: app_public_args + app_private_args,
    dependencies: app_deps,
)

app_dep = declare_dependency(
    link_with: app_lib,
    include_directories: include_directories('.'),
    compile_args: app_public_args,
    dependencies: app_deps,
)

# link_whole sidesteps the ui<->editor<->plugins link cycles; the test suite
# reuses both lists below.
pawmmit_module_libs = [
    app_lib,
    ui_lib,
    editor_lib,
    plugins_lib,
    dialogs_lib,
    tools_lib,
    update_lib,
    watcher_lib,
    host_lib,
    index_lib,
    conf_lib,
    cred_lib,
    git_lib,
    loglib_lib,
    cli_lib,
    pawmmitUtil_lib,
    translation_lib,
    platform_lib,
]
pawmmit_link_deps = [
    pawmmit_common,
    libgit2_dep,
    libssh2_dep,
    lua_dep,
    lpeg_dep,
    hunspell_dep,
    cmark_dep,
    scintillua_dep,
    scintilla_dep,
    lexilla_dep,
]

if is_win
    pawmmit_link_deps += [cc.find_library('dbghelp')]
endif

exe_kwargs = {}
exe_sources = [files('Pawmmit.cpp')]

if is_win
    windows = import('windows')
    exe_sources += windows.compile_resources(
        rsrc_dir / 'Pawmmit.rc',
        include_directories: include_directories('..' / '..' / 'rsrc'),
    )
    exe_kwargs += {'win_subsystem': 'windows'}
    # win_subsystem:'windows' makes the linker pull in MSVCRT's WinMain-based
    # startup stub, which needs Qt's own WinMain (it forwards to our int
    # main()). Qt's official CMake package links this automatically; meson's
    # qmake-based Qt detection doesn't, so it's found and linked explicitly.
    _qt_libdir = run_command(_qtpaths, '--query', 'QT_INSTALL_LIBS', check: true).stdout().strip()
    qt_entrypoint_dep = cc.find_library('Qt6EntryPoint', dirs: [_qt_libdir])
    exe_kwargs += {'dependencies': pawmmit_link_deps + [qt_entrypoint_dep]}
else
    exe_kwargs += {'dependencies': pawmmit_link_deps}
endif

if is_mac
    exe_kwargs += {'install_rpath': '@executable_path/../Frameworks'}
elif not is_win and get_option('bundled')
    exe_kwargs += {'install_rpath': '$ORIGIN'}
endif

pawmmit_exe = executable(
    'pawmmit',
    exe_sources,
    cpp_args: app_public_args,
    link_whole: pawmmit_module_libs,
    link_depends: doc_html,
    install: true,
    install_dir: app_bindir,
    kwargs: exe_kwargs,
)

# macOS bundle scaffolding: Info.plist + icon inside pawmmit.app/Contents.
if is_mac
    plist_conf = configuration_data()
    plist_conf.set('MACOSX_BUNDLE_EXECUTABLE_NAME', 'pawmmit')
    plist_conf.set('MACOSX_BUNDLE_BUNDLE_NAME', 'Pawmmit')
    plist_conf.set('MACOSX_BUNDLE_GUI_IDENTIFIER', 'com.github.Pawmmit.Pawmmit')
    plist_conf.set('MACOSX_BUNDLE_ICON_FILE', 'Pawmmit')
    plist_conf.set('MACOSX_BUNDLE_BUNDLE_VERSION', meson.project_version())
    plist_conf.set('MACOSX_BUNDLE_LONG_VERSION_STRING', meson.project_version())
    plist_conf.set('MACOSX_BUNDLE_SHORT_VERSION_STRING', meson.project_version())
    plist_conf.set('MACOSX_BUNDLE_INFO_STRING', 'Pawmmit ' + meson.project_version())
    plist_conf.set('MACOSX_BUNDLE_COPYRIGHT', 'Pawmmit Contributors')

    configure_file(
        input: rsrc_dir / 'Info.plist.in',
        output: 'Info.plist',
        configuration: plist_conf,
        format: 'cmake',
        install: true,
        install_dir: app_contents,
    )
    install_data(rsrc_dir / 'Pawmmit.icns', install_dir: resources_dir)
endif

# Resource files loaded from resources_dir at runtime.
install_data(
    files(
        conf_dir / 'emoji.json',
        conf_dir / 'mergetools',
        conf_dir / 'credential.lua',
        conf_dir / 'editor.lua',
        conf_dir / 'global.lua',
        conf_dir / 'lexers.lua',
        conf_dir / 'merge.lua',
        conf_dir / 'terminal.lua',
        conf_dir / 'update.lua',
        conf_dir / 'window.lua',
    ),
    install_dir: resources_dir,
)
install_subdir(conf_dir / 'dictionaries', install_dir: resources_dir)
install_subdir(conf_dir / 'themes', install_dir: resources_dir)
install_subdir(conf_dir / 'plugins', install_dir: resources_dir)
install_subdir(
    scintillua_dep.get_variable(internal: 'lexers_dir'),
    install_dir: resources_dir / 'lexers',
    strip_directory: true,
)

if not is_mac and not is_win
    install_data(
        rsrc_dir / 'linux' / 'com.github.Pawmmit.Pawmmit.desktop',
        install_dir: get_option('datadir') / 'applications',
    )

    appdata_conf = configuration_data()
    appdata_conf.set('PAWMMIT_IDENTIFIER', 'com.github.Pawmmit.Pawmmit')
    appdata_configured = configure_file(
        input: rsrc_dir / 'linux' / 'com.github.Pawmmit.Pawmmit.appdata.xml.in',
        output: 'com.github.Pawmmit.Pawmmit.appdata.xml.configured',
        configuration: appdata_conf,
    )

    # Merges CHANGELOG.md into a <releases> block, or omits it if empty.
    appstreamcli = find_program('appstreamcli')
    custom_target(
        'com.github.Pawmmit.Pawmmit.appdata.xml',
        input: [meson.project_source_root() / 'CHANGELOG.md', appdata_configured],
        output: 'com.github.Pawmmit.Pawmmit.appdata.xml',
        # --format text avoids appstreamcli's autodetection
        command: [
            appstreamcli,
            'news-to-metainfo',
            '--format', 'text',
            '@INPUT0@',
            '@INPUT1@',
            '@OUTPUT@',
        ],
        build_by_default: true,
        install: true,
        install_dir: get_option('datadir') / 'metainfo',
    )
endif