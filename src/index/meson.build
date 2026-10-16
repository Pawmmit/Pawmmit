index_src = [
    'FTS.cpp',
    'GenericLexer.cpp',
    'Index.cpp',
    'IndexDatabase.cpp',
    'IndexerProcess.cpp',
    'IndexModel.cpp',
    'IndexWriter.cpp',
    'Lexer.cpp',
    'LPegLexer.cpp',
    'Query.cpp',
]

index_moc = qt6.preprocess(
    moc_headers: files('Index.h', 'IndexerProcess.h', 'IndexModel.h'),
    dependencies: [qt6_dep, pawmmit_common],
)

index_rsrc = qt6.compile_resources(name: 'index', sources: 'index.qrc')

index_lib = static_library(
    'index',
    index_src + index_moc + index_rsrc,
    dependencies: [pawmmit_common, pawmmitUtil_dep, conf_dep, git_dep, lpeg_dep, sqlite_dep],
)

index_dep = declare_dependency(
    link_with: index_lib,
    include_directories: include_directories('.'),
    dependencies: [pawmmitUtil_dep, conf_dep, git_dep, lpeg_dep, sqlite_dep],
)

executable('lexer_test', 'lexer_test.cpp', dependencies: [pawmmit_common, index_dep])
executable('index_test', 'index_test.cpp', dependencies: [pawmmit_common, index_dep])

subdir('Indexer')