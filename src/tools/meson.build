tools_moc = qt6.preprocess(
    moc_headers: files(
        'DiffTool.h',
        'EditTool.h',
        'ExternalTool.h',
        'MergeTool.h',
        'ShowTool.h',
    ),
    dependencies: [qt6_dep, pawmmit_common],
)

tools_src = files(
    'DiffTool.cpp',
    'EditTool.cpp',
    'ExternalTool.cpp',
    'MergeTool.cpp',
    'ShowTool.cpp',
)

tools_lib = static_library(
    'tools',
    tools_src + tools_moc,
    dependencies: [pawmmit_common, pawmmitUtil_dep, conf_dep, git_dep, platform_dep],
)

tools_dep = declare_dependency(
    link_with: tools_lib,
    include_directories: include_directories('.'),
)