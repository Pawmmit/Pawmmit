platform_src = [
    'HostProcess.cpp',
]

platform_lib = static_library('platform', platform_src, dependencies: [pawmmit_common, pawmmitUtil_dep])

platform_dep = declare_dependency(
    link_with: platform_lib,
    include_directories: include_directories('.'),
)