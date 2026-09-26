//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#ifndef PLATFORM_FILEMANAGER_H
#define PLATFORM_FILEMANAGER_H

#include <QString>

namespace platform {

/// @brief Get the platform's default file manager command
/// @return Command with %1 in place of the path
QString defaultFileManagerCommand();

/// @brief Open a directory with the file manager command
/// @param command Command with %1 in place of the path
/// @param path Directory to open
/// @return Whether it could be started
bool openFileManager(const QString &command, const QString &path);

/// @brief Show a file selected in the file manager
/// @param file File or directory to show
/// @param command Configured file manager command, or empty for the default
/// @return Whether it could be started
bool revealInFileManager(const QString &file, const QString &command);

} // namespace platform

#endif
