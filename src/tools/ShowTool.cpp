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
#include "platform/FileManager.h"

#if defined(Q_OS_MAC)
#define NAME QT_TRANSLATE_NOOP("ShowTool", "Finder")
#elif defined(Q_OS_WIN)
#define NAME QT_TRANSLATE_NOOP("ShowTool", "Explorer")
#else
#define NAME QT_TRANSLATE_NOOP("ShowTool", "Default File Browser")
#endif

namespace {

QString configuredFileManagerCommand() {
  return Settings::instance()
      ->value(Setting::Id::FilemanagerCommand)
      .toString();
}

} // namespace

bool ShowTool::openFileManager(QString path) {
  QString command = configuredFileManagerCommand();
  if (command.isEmpty())
    command = platform::defaultFileManagerCommand();
  return platform::openFileManager(command, path);
}

ShowTool::ShowTool(const QString &file, QObject *parent)
    : ExternalTool(file, parent) {}

ExternalTool::Kind ShowTool::kind() const { return Show; }

QString ShowTool::name() const { return tr("Show in %1").arg(tr(NAME)); }

bool ShowTool::start() {
  return platform::revealInFileManager(mFile, configuredFileManagerCommand());
}
