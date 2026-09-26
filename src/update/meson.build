update_moc = qt6.preprocess(
    moc_headers: files(
        'DownloadDialog.h',
        'UpdateDialog.h',
        'UpToDateDialog.h',
        'Updater.h',
    ),
    dependencies: [qt6_dep, pawmmit_common],
)

update_src = files(
    'DownloadDialog.cpp',
    'UpdateDialog.cpp',
    'Updater.cpp',
    'UpToDateDialog.cpp',
)

update_deps = [pawmmit_common, pawmmitUtil_dep, conf_dep, git_dep, cmark_dep, platform_dep]

if is_mac
    update_src += files('Updater_mac.mm')
    update_deps += dependency('appleframeworks', modules: ['AppKit', 'Foundation'])
elif is_win
    update_src += files('Updater_win.cpp')
endif

update_lib = static_library(
    'update',
    update_src + update_moc,
    dependencies: update_deps,
)

update_dep = declare_dependency(
    link_with: update_lib,
    include_directories: include_directories('.'),
)

# Standalone helper that relaunches the app after an update. Installed next to
# the main executable on every platform (Contents/MacOS on macOS).
relauncher_kwargs = {}
if is_mac
    relauncher_kwargs += {'install_rpath': '@loader_path/../Frameworks'}
elif not is_win and get_option('bundled')
    relauncher_kwargs += {'install_rpath': '$ORIGIN'}
endif

executable(
    'pawmmit-relauncher',
    'relauncher.cpp',
    dependencies: dependency('qt6', modules: ['Core']),
    install: true,
    install_dir: app_bindir,
    kwargs: relauncher_kwargs,
)
