Version 0.2.0
-------------
Released: 2026-10-10

Features:
 * Add opt-in support for LuaJIT
 * Add AI translated Norwegian (Bokmål)
 * Add a status bar
 * Improve guidance regarding merge, rebase, stash etc.
 * Misc. Lua related improvements
 * Reduce the number of notifications and refreshes on macOS by adding file notification
 * Remove context lines from the indexer and index search
 * Remove the terminal tab from application settings
 * Upgrade to lexilla 5.5.4 and scintilla 5.6.7

Bugfix:
 * Fix multiple issues from the scintilla upgrade
 * Fix issues loading hunks if the resolution is high
 * Fix a number of usability issues and inconsistencies in the GUI
 * Fix scaling issues with HiDPI
 * Fix various focus related issues
 * Fix duplicate entries of 'External Diff to Working Copy' in context menu
 * Fix scintilla integration issues
 * Fix an issue where NUL bytes are potentially dropped
 * Fix various translation issues
 * Fix icon drawing issue with different HiDPI screens
 * Fix usability issues with stashing
 * Fix settings being dropped in RepoView callbacks
 * Fix the indexer not starting in a development environment
 * Fix indexer not responding to cancel
 * Fix an issue on macOS with un-escaped path being passed to shell
 * Fix various issues with CI pipeline to stabilise it and make it easier to debug

Version 0.1.2
-------------
Released: 2026-09-19

Features:
 * Add CI unit testing for macOS
 * Add smoke testing for Linux, macOS and Windows

Bugfix:
 * Fix packaging issues for macOS
 * Fix misc. issues causing flaky CI pipeline
 * Fix several issues related file updates being missed

Version 0.1.1
-------------
Released: 2026-09-19

Bugfix:
 * Fix packaging issues for AppImage and Windows

Version 0.1.0
-------------
Released: 2026-09-18

Features:
 * Rebrand fork to Pawmmit
 * Built upon latest master branch of Gittyup
 * Initial attempts to improve first-time user experience
 * Option to save and apply patches

Miscellaneous:
 * Codebase ported to build with meson
 * Simplified some GUI elements

Bugfix:
 * Misc. crash fixes
 * Window geometry not always restored when opening the application
 * GUI hanging whilst loading the diff
