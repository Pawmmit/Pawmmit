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

#include "EditTool.h"
#include "git/Config.h"
#include "git/Repository.h"
#include "platform/HostProcess.h"
#include "util/Path.h"
#include <QDesktopServices>
#include <QUrl>

EditTool::EditTool(const QString &file, QObject *parent)
    : ExternalTool(file, parent) {}

bool EditTool::isValid() const {
  return (ExternalTool::isValid() && QFileInfo(mFile).isFile());
}

ExternalTool::Kind EditTool::kind() const { return Edit; }

QString EditTool::name() const { return tr("Edit in External Editor"); }

bool EditTool::start() {
  // Resolve potentially sandboxed path
  const QString file = util::sandboxPathToHost(mFile);

  git::Config config = git::Config::global();
  QString editor = config.value<QString>("gui.editor");

  if (editor.isEmpty())
    editor = qgetenv("GIT_EDITOR");

  if (editor.isEmpty())
    editor = config.value<QString>("core.editor");

  if (editor.isEmpty())
    editor = qgetenv("VISUAL");

  if (editor.isEmpty())
    editor = qgetenv("EDITOR");

  if (editor.isEmpty())
    return QDesktopServices::openUrl(QUrl::fromLocalFile(file));

  // Find arguments.
  QStringList args = editor.split("\" \"");

  if (args.count() > 1) {
    // Format 1: "Command" "Argument1" "Argument2"
    editor = args[0];
    for (int i = 1; i < args.count(); i++)
      args[i].remove("\"");
  } else {
    int fi = editor.indexOf("\"");
    int li = editor.lastIndexOf("\"");
    if ((fi == 0) && (li > fi) && (li < (editor.length() - 1))) {
      // Format 2: "Command" Argument1 Argument2
      args = editor.right(editor.length() - li - 2).split(" ");
      args.insert(0, "dummy");
      editor = editor.left(li + 1);
    } else {
      // Format 3: "Command" (no argument)
      // Format 4: Command (no argument)
    }
  }

  // Remove command, add filename, trim command.
  args.removeFirst();
  args.append(file);
  editor.remove("\"");

  // Destroy this after process finishes.
  platform::HostProcess *process = new platform::HostProcess(this);
  auto signal = QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished);
  QObject::connect(&process->process(), signal, this,
                   &ExternalTool::deleteLater);

  process->start(editor, args);

  if (!process->waitForStarted())
    return false;

  // Detach from parent.
  setParent(nullptr);

  return true;
}
