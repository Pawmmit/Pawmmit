//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "FileManager.h"
#include "HostProcess.h"
#include "util/Path.h"
#include <QDir>
#include <QStringList>

namespace platform {

bool openFileManager(const QString &command, const QString &path) {
  QStringList cmdParts = QProcess::splitCommand(command);
  // Resolve potentially sandboxed path
  QString hostPath = QDir::toNativeSeparators(util::sandboxPathToHost(path));

  for (QString &part : cmdParts)
    part = part.arg(hostPath);

  if (cmdParts.isEmpty())
    return false;

  QString program = cmdParts.takeFirst();
  HostProcess process;
  return process.startDetached(program, cmdParts);
}

} // namespace platform
