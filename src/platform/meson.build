terminal_impl = {
    'darwin': 'Terminal_mac.cpp',
    'windows': 'Terminal_win.cpp',
}.get(host_machine.system(), 'Terminal_unix.cpp')

filemanager_impl = {
    'darwin': 'FileManager_mac.cpp',
    'windows': 'FileManager_win.cpp',
}.get(host_machine.system(), 'FileManager_unix.cpp')

platform_src = [
    'FileManager.cpp',
    'GitInstall.cpp',
    'HostProcess.cpp',
    filemanager_impl,
    terminal_impl,
]

platform_lib = static_library('platform', platform_src, dependencies: [pawmmit_common, pawmmitUtil_dep])

platform_dep = declare_dependency(
    link_with: platform_lib,
    include_directories: include_directories('.'),
)
