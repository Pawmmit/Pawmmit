cred_src = [
    'Cache.cpp',
    'CredentialHelper.cpp',
    'GitCredential.cpp',
    'Store.cpp',
]

cred_moc = qt6.preprocess(
    moc_headers: files('CredentialHelper.h'),
    dependencies: [qt6_dep, pawmmit_common],
)

cred_lib = static_library(
    'cred',
    cred_src + cred_moc,
    dependencies: [pawmmit_common, conf_dep, git_dep, platform_dep])

cred_dep = declare_dependency(
    link_with: cred_lib,
    include_directories: include_directories('.'),
)