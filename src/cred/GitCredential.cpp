//
//          Copyright (c) 2018, Scientific Toolworks, Inc.
//
// This software is licensed under the GNU General Public License v3.0 or
// (at your option) any later version. The LICENSE.md file describes the
// conditions under which this software may be distributed.
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Author: Jason Haslam
//

#include "GitCredential.h"
#include "qtsupport.h"
#include "platform/GitInstall.h"
#include <QStandardPaths>
#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QTextStream>
#include <QUrl>

namespace {

QString host(const QString &url) {
  QString host = QUrl(url).host();
  if (!host.isEmpty())
    return host;

  // Extract hostname from SSH URL.
  int end = url.indexOf(':');
  int begin = url.indexOf('@') + 1;
  return url.mid(begin, end - begin);
}

QString protocol(const QString &url) {
  QString scheme = QUrl(url).scheme();
  return !scheme.isEmpty() ? scheme : "ssh";
}

} // namespace

GitCredential::GitCredential(const QString &name) : mName(name) {}

bool GitCredential::get(const QString &url, QString &username,
                        QString &password) {
  QProcess process;
  process.start(command(), {"get"});
  if (!process.waitForStarted())
    return false;

  QTextStream out(&process);
  out << "protocol=" << protocol(url) << Qt::endl;
  out << "host=" << host(url) << Qt::endl;
  if (!username.isEmpty())
    out << "username=" << username << Qt::endl;
  out << Qt::endl;

  process.closeWriteChannel();
  process.waitForFinished();

  QString output = process.readAllStandardOutput();
  for (const QString &line : output.split('\n')) {
    int pos = line.indexOf('=');
    if (pos < 0)
      continue;

    QString key = line.left(pos);
    QString value = line.mid(pos + 1);
    if (key == "username") {
      username = value;
    } else if (key == "password") {
      password = value;
    }
  }

  return !username.isEmpty() && !password.isEmpty();
}

bool GitCredential::store(const QString &url, const QString &username,
                          const QString &password) {
  QProcess process;
  process.start(command(), {"store"});
  if (!process.waitForStarted())
    return false;

  QTextStream out(&process);
  out << "protocol=" << protocol(url) << Qt::endl;
  out << "host=" << host(url) << Qt::endl;
  out << "username=" << username << Qt::endl;
  out << "password=" << password << Qt::endl;
  out << Qt::endl;

  process.closeWriteChannel();
  process.waitForFinished();

  return true;
}

QString GitCredential::command() const {
  QString name = QString("git-credential-%1").arg(mName);
  QDir appDir = QCoreApplication::applicationDirPath();
  appDir.cd("credential-helpers");

  // Prefer credential helpers directly installed into Pawmmit's app dir
  QString candidate =
      QStandardPaths::findExecutable(name, QStringList(appDir.path()));
  if (!candidate.isEmpty()) {
    return candidate;
  }

  candidate = QStandardPaths::findExecutable(name);
  if (!candidate.isEmpty()) {
    return candidate;
  }

  // Look in the helpers bundled with Git for Windows.
  QString gitDir = platform::gitInstallDir();
  if (!gitDir.isEmpty()) {
    QDir dir(gitDir);
    candidate = QStandardPaths::findExecutable(
        name, {dir.filePath("mingw64/bin"), dir.filePath("mingw32/bin")});
    if (!candidate.isEmpty()) {
      return candidate;
    }
  }

  return name;
}
