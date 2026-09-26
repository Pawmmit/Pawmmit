git_src = [
    'AnnotatedCommit.cpp',
    'Blame.cpp',
    'Blob.cpp',
    'Branch.cpp',
    'Command.cpp',
    'Commit.cpp',
    'Config.cpp',
    'Diff.cpp',
    'Filter.cpp',
    'FilterList.cpp',
    'Id.cpp',
    'Index.cpp',
    'Object.cpp',
    'Patch.cpp',
    'Rebase.cpp',
    'Reference.cpp',
    'Remote.cpp',
    'Repository.cpp',
    'Result.cpp',
    'RevWalk.cpp',
    'Signature.cpp',
    'Submodule.cpp',
    'Tag.cpp',
    'TagRef.cpp',
    'Tree.cpp',
]

git_public_args = [
    '-DUSE_SYSTEM_LIBGIT2'
]

# RepositoryNotifier (in Repository.h) is the only Q_OBJECT class here.
git_moc = qt6.preprocess(
    moc_headers: files('Repository.h'),
    dependencies: [qt6_dep, pawmmit_common],
)

git_lib = static_library(
    'git',
    git_src + git_moc,
    cpp_args: git_public_args,
    dependencies: [pawmmit_common, libgit2_dep, libssh2_dep, pawmmitUtil_dep, platform_dep])

git_dep = declare_dependency(
    link_with: git_lib,
    include_directories: include_directories('.'),
    compile_args: git_public_args,
    # Remote.cpp calls libssh2 directly; consumers that link git need it too.
    dependencies: [libgit2_dep, libssh2_dep],
)

