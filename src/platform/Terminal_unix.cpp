//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "Terminal.h"
#include "HostProcess.h"
#include "Debug.h"
#include <QStringList>

namespace platform {

QString defaultTerminalCommand() {
  static QString detectedTerminal = nullptr;
  static const QStringList candidates = {
      "x-terminal-emulator", "xdg-terminal", "i3-sensible-terminal",
      "gnome-terminal",      "konsole",      "xterm",
  };

  if (!detectedTerminal.isNull())
    return detectedTerminal;

  detectedTerminal = "";

  for (const QString &candidate : candidates) {
    QString exePath = HostProcess::findExecutable(candidate);
    if (!exePath.isEmpty()) {
      detectedTerminal =
          '"' + exePath.replace("\\", "\\\\").replace("\"", "\\\"") + '"';
      break;
    }
  }

  return detectedTerminal;
}

bool openTerminal(const QString &command, const QString &dir) {
  HostProcess child;
  child.setWorkingDirectory(dir);
  bool started = child.startDetached("sh", {"-c", command});
  Debug("Execute Terminal: Arguments: " << child.process().arguments());
  return started;
}

} // namespace platform
