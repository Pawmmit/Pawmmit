//
//          Copyright (c) 2017, Scientific Toolworks, Inc.
//
// This software is licensed under the GNU General Public License v3.0 or
// (at your option) any later version. The LICENSE.md file describes the
// conditions under which this software may be distributed.
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Author: Jason Haslam
//

#include "Command.h"
#include "platform/GitInstall.h"
#include <QDir>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>

namespace git {

QString Command::bashPath() {
  QStringList paths;
  QString gitDir = platform::gitInstallDir();
  if (!gitDir.isEmpty())
    paths.append(QDir(gitDir).filePath("bin"));

  return QStandardPaths::findExecutable("bash", paths);
}

QString Command::substitute(const QProcessEnvironment &env,
                            const QString &command) {
  QList<QRegularExpressionMatch> matches;
  QRegularExpression re("\\$\\{?([_a-zA-Z]\\w+)\\}?");
  QRegularExpressionMatchIterator it = re.globalMatch(command);
  while (it.hasNext())
    matches.prepend(it.next());

  // Substitute in reverse order.
  QString result = command;
  for (const QRegularExpressionMatch &match : matches) {
    QString value = env.value(match.captured(1));
    result.replace(match.capturedStart(), match.capturedLength(), value);
  }

  return result;
}

} // namespace git
