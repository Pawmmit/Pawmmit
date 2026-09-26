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

#include "ShowTool.h"
#include "conf/Settings.h"
#include "git/Repository.h"
#include "platform/HostProcess.h"
#include "util/Path.h"
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QUrl>

#if defined(Q_OS_MAC)
#define NAME QT_TRANSLATE_NOOP("ShowTool", "Finder")
#elif defined(Q_OS_WIN)
#define NAME QT_TRANSLATE_NOOP("ShowTool", "Explorer")
#else
#define NAME QT_TRANSLATE_NOOP("ShowTool", "Default File Browser")
#endif

bool ShowTool::openFileManager(QString path) {
  QString fileManagerCmd =
      Settings::instance()->value(Setting::Id::FilemanagerCommand).toString();

  if (fileManagerCmd.isEmpty()) {
#if defined(Q_OS_WIN)
    fileManagerCmd = "explorer \"%1\"";

#elif defined(Q_OS_MACOS)
    fileManagerCmd = "open \"%1\"";

#elif defined(Q_OS_UNIX)
    fileManagerCmd = "xdg-open \"%1\"";
#endif
  }

  QStringList cmdParts = QProcess::splitCommand(fileManagerCmd);
  // Resolve potentially sandboxed path
  path = QDir::toNativeSeparators(util::sandboxPathToHost(path));

  for (QString &part : cmdParts)
    part = part.arg(path);

  if (cmdParts.isEmpty()) {
    return false;
  } else {
    QString program = cmdParts.takeFirst();
    platform::HostProcess process;
    return process.startDetached(program, cmdParts);
  }
}

ShowTool::ShowTool(const QString &file, QObject *parent)
    : ExternalTool(file, parent) {}

ExternalTool::Kind ShowTool::kind() const { return Show; }

QString ShowTool::name() const { return tr("Show in %1").arg(tr(NAME)); }

bool ShowTool::start() {
#if defined(Q_OS_MAC)
  return QProcess::startDetached(
      "/usr/bin/osascript", {"-e", "tell application \"Finder\"", "-e",
                             QString("reveal POSIX file \"%1\"").arg(mFile),
                             "-e", "activate", "-e", "end tell"});
#elif defined(Q_OS_WIN)
  return QProcess::startDetached("explorer.exe",
                                 {"/select,", QDir::toNativeSeparators(mFile)});
#else
  QFileInfo info(mFile);
  return openFileManager(info.isDir() ? info.filePath() : info.path());
#endif
}
