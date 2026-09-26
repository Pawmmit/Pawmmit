//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "FileManager.h"
#include <QFileInfo>

namespace platform {

QString defaultFileManagerCommand() { return "xdg-open \"%1\""; }

bool revealInFileManager(const QString &file, const QString &command) {
  // There's no common way to select a file, so open its directory instead.
  QFileInfo info(file);
  return openFileManager(command, info.isDir() ? info.filePath() : info.path());
}

} // namespace platform
