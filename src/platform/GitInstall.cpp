//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "GitInstall.h"
#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QStandardPaths>

namespace platform {

QString gitInstallDir() {
#ifdef Q_OS_WIN
  QString git = QStandardPaths::findExecutable("git");
  if (git.isEmpty()) {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (const QString &var : {"PROGRAMFILES", "PROGRAMFILES(x86)"}) {
      QString programFiles = env.value(var);
      QFileInfo info(QDir(programFiles).filePath("Git/cmd/git.exe"));
      if (!programFiles.isEmpty() && info.exists()) {
        git = info.filePath();
        break;
      }
    }
  }

  if (git.isEmpty())
    return QString();

  // git.exe lives in cmd, bin or mingw64/bin below the installation root.
  QDir dir = QFileInfo(git).dir();
  if (dir.dirName() != "cmd" && dir.dirName() != "bin")
    return QString();

  dir.cdUp();
  if (dir.dirName().startsWith("mingw"))
    dir.cdUp();

  return dir.path();
#else
  return QString();
#endif
}

} // namespace platform
