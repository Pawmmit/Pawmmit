//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "FileManager.h"
#include <QDir>
#include <QProcess>

namespace platform {

QString defaultFileManagerCommand() { return "explorer \"%1\""; }

bool revealInFileManager(const QString &file, const QString &command) {
  Q_UNUSED(command)
  return QProcess::startDetached("explorer.exe",
                                 {"/select,", QDir::toNativeSeparators(file)});
}

} // namespace platform
