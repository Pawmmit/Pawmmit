//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "FileManager.h"
#include <QProcess>

namespace platform {

QString defaultFileManagerCommand() { return "open \"%1\""; }

bool revealInFileManager(const QString &file, const QString &command) {
  Q_UNUSED(command)
  return QProcess::startDetached("/usr/bin/osascript",
                                 {"-e", "tell application \"Finder\"", "-e",
                                  QString("reveal POSIX file \"%1\"").arg(file),
                                  "-e", "activate", "-e", "end tell"});
}

} // namespace platform
