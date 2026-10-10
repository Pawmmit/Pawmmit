project(
    'pawmmit',
    ['cpp', 'c'],
    version: '0.2.0',
    meson_version: '>= 1.3',
    # buildtype=release alone doesn't define NDEBUG, but confDir()/l10nDir()
    # need it to prefer installed paths. The fallback Lua would otherwise be a
    # shared library that no packaging step bundles. Lua errors unwind through
    # extern "C" functions, which MSVC's default /EHsc assumes never throw.
    default_options: [
        'cpp_std=c++17',
        'c_std=c99',
        'warning_level=1',
        'b_ndebug=if-release',
        'lua:default_library=static',
        'cpp_eh=s',
    ],
)

qt6 = import('qt6')
fs = import('fs')
cc = meson.get_compiler('cpp')

host_sys = host_machine.system()
is_mac = host_sys == 'darwin'
is_win = host_sys == 'windows'

# Objective-C++ for the *_mac.mm sources (cli, app).
cpp_like_langs = ['cpp']
if is_mac
    add_languages('objcpp', native: false)
    cpp_like_langs += 'objcpp'
endif

# Project-wide feature defines.
_defs = []
if get_option('debug_output')
    if get_option('debug_output_general')
        _defs += '-DDEBUG_OUTPUT_GENERAL'
    endif
    if get_option('debug_output_refresh')
        _defs += '-DDEBUG_OUTPUT_REFRESH'
    endif
else
    _defs += '-DQT_NO_DEBUG_OUTPUT'
endif
if get_option('flatpak')
    _defs += '-DFLATPAK'
endif
if get_option('debug_flatpak')
    _defs += '-DDEBUG_FLATPAK'
endif
if get_option('enable_update_over_gui')
    _defs += '-DENABLE_UPDATE_OVER_GUI'
endif
if get_option('enable_sha256')
    _defs += '-DGIT_EXPERIMENTAL_SHA256'
endif

# cmake and/or Qt in cmake seems to inject these defines in that we depend on
if is_win
    _defs += ['-DUNICODE', '-D_UNICODE']
endif
add_project_arguments(_defs, language: cpp_like_langs)

# -Wall comes from warning_level=1; -Werror is left to the built-in `werror`
# option (current GCC / Qt headers raise warnings older toolchains do not).
if cc.get_argument_syntax() == 'gcc'
    add_project_arguments('-Wno-unused-parameter', language: ['c'] + cpp_like_langs)

    # GCC 16's -Wsfinae-incomplete fires inside Qt's own <QtCore/QtCore> umbrella
    # header. Ignore it until Qt is fixed
    if cc.has_argument('-Wno-sfinae-incomplete')
        add_project_arguments('-Wno-sfinae-incomplete', language: cpp_like_langs)
    endif
endif

# libgit2's qsort_r pulls in dlopen on Linux (folded into libc on current glibc,
# so this is only needed on older systems).
if not is_mac and not is_win
    _dl = cc.find_library('dl', required: false)
    if _dl.found()
        add_project_link_arguments('-ldl', language: 'cpp')
    endif
endif

cmark_dep = dependency('libcmark')
hunspell_dep = dependency('hunspell', version: '>=1.7')
lexilla_dep = dependency('lexilla')

# System-only: Meson's CMake bridge can't reconstruct libgit2's own build.
# Windows/macOS get it via vcpkg/Homebrew (see build.yml); Flatpak builds it
# directly with CMake. vcpkg ships no .pc file, so Windows resolves it via
# CMake's own exported target, which libgit2 names libgit2::libgit2package.
if is_win
    libgit2_dep = dependency(
        'libgit2',
        version: '>=1.9',
        method: 'cmake',
        modules: ['libgit2::libgit2package'],
    )
else
    libgit2_dep = dependency('libgit2', version: '>=1.9')
endif

libssh2_dep = dependency('libssh2')

luajit_dep = dependency('luajit', version: '>=2.1', required: get_option('luajit'))
if luajit_dep.found()
    lua_dep = declare_dependency(
        dependencies: [luajit_dep, dependency('lua-compat-5.3')],
        version: '5.1',
    )
    meson.override_dependency('lua', lua_dep)
else
    #Find system installed Lua on a wider range of distros
    lua_dep = dependency(
        'lua-5.5',
        'lua-5.4',
        'lua-5.3',
        version: '>=5.3',
        required: false,
        allow_fallback: false,
    )
    if lua_dep.found()
        meson.override_dependency('lua', lua_dep)
    else
        lua_dep = dependency('lua', version: '>=5.3')
    endif
endif
lpeg_dep = dependency('lpeg')

scintilla_dep = dependency('scintilla')
scintillua_dep = dependency('scintillua')
qt_modules = ['Core', 'Concurrent', 'Core5Compat', 'Gui', 'Network', 'Widgets']
if not is_mac and not is_win
    qt_modules += 'DBus' # only reached from Q_OS_LINUX code paths
endif
qt6_dep = dependency('qt6', version: '>=6.7', modules: qt_modules)

# Where Qt keeps its own qtbase_*.qm files, used by l10n and at runtime.
_qtpaths = find_program('qtpaths6', 'qtpaths', 'qtpaths-qt6', required: false)
qt_translations_dir = '/usr/share/qt6/translations'
if _qtpaths.found()
    _r = run_command(_qtpaths, '--query', 'QT_INSTALL_TRANSLATIONS', check: false)
    if _r.returncode() == 0 and _r.stdout().strip() != ''
        qt_translations_dir = _r.stdout().strip()
    endif
endif

# Meson adds no rpath for Qt frameworks, so executables run from the build
# directory can't load them without one.
qt_build_rpath = ''
if is_mac and _qtpaths.found()
    _libs = run_command(_qtpaths, '--query', 'QT_INSTALL_LIBS', check: false)
    if _libs.returncode() == 0
        qt_build_rpath = _libs.stdout().strip()
    endif
endif

# Install layout differs per platform. On macOS everything lands inside the
# .app bundle.
if is_mac
    app_contents = 'pawmmit.app' / 'Contents'
    app_bindir = app_contents / 'MacOS'
    resources_dir = app_contents / 'Resources'
    l10n_install_dir = resources_dir / 'l10n'
elif is_win
    app_contents = ''
    app_bindir = get_option('bindir')
    resources_dir = 'Resources'
    l10n_install_dir = 'Resources' / 'l10n'
else
    app_contents = ''
    app_bindir = get_option('bindir')
    resources_dir = get_option('datadir') / 'Pawmmit'
    l10n_install_dir = get_option('localedir') / 'Pawmmit'
endif

# Empty for a normal build; a non-empty dev_build option turns into a suffix
# shown after the application name in window titles and --help.
dev_build = get_option('dev_build')
build_description = dev_build == '' ? '' : ' (development build: ' + dev_build + ')'

# Build stamp for the About dialog; refreshes on reconfigure, not every build.
git = find_program('git', required: false)
build_revision = 'unknown'
if git.found()
    git_show = run_command(git, 'show', '-s', '--format=%h', 'HEAD', check: false)
    if git_show.returncode() == 0
        build_revision = git_show.stdout().strip()
    endif
endif

python = find_program('python3', 'python')

version_config = configuration_data()
version_config.set(
    'CURR_YEAR',
    run_command(
        python,
        '-c', 'import datetime; print(datetime.date.today().year)',
        check: true,
    ).stdout().strip(),
)
version_config.set_quoted('PAWMMIT_BUILD_REVISION', build_revision)
version_config.set_quoted(
    'PAWMMIT_BUILD_DATE',
    run_command(
        python,
        '-c', 'import datetime; print(datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"))',
        check: true,
    ).stdout().strip(),
)

version_h = configure_file(output: 'version.h', configuration: version_config)
version_dep = declare_dependency(
    sources: version_h,
    include_directories: include_directories('.'),
)

# Plain version string for external consumers (CI reads it back for releases).
configure_file(
    output: 'Version.txt',
    command: [python, '-c', 'print("' + meson.project_version() + '", end="")'],
    capture: true,
)

subdir('l10n')
subdir('src')
subdir('pack')

#Place all the installable executables in the root build directory
# This is both more conventient for development and also makes
# the location representative compared to the installation
executable(
    'pawmmit',
    exe_sources,
    cpp_args: app_public_args,
    link_whole: pawmmit_module_libs,
    link_depends: doc_html,
    install: true,
    install_dir: app_bindir,
    kwargs: exe_kwargs,
)

# Installed next to the main executable on every platform (Contents/MacOS on
# macOS) - the app spawns it as a sibling.
indexer_exe = executable(
    'pawmmit-indexer',
    indexer_src,
    cpp_args: ['-DPAWMMIT_VERSION="' + meson.project_version() + '"'],
    dependencies: indexer_deps,
    install: true,
    install_dir: app_bindir,
    kwargs: indexer_kwargs,
)

executable(
    'pawmmit-relauncher',
    'src/update/relauncher.cpp',
    dependencies: dependency('qt6', modules: ['Core']),
    install: true,
    install_dir: app_bindir,
    kwargs: relauncher_kwargs,
)

if get_option('tests')
    subdir('test')
endif