//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "Terminal.h"
#include "HostProcess.h"
#include "Debug.h"

namespace platform {

QString defaultTerminalCommand() {
  static QString detectedTerminal = nullptr;
  static const char *candidates[] = {"com.googlecode.iterm2",
                                     "com.apple.Terminal", nullptr};

  if (!detectedTerminal.isNull())
    return detectedTerminal;

  detectedTerminal = "";

  for (const char **candidate = candidates; *candidate; ++candidate) {
    int res = QProcess::execute(
        "osascript", {"-e", QString("tell application \"Finder\" to get "
                                    "application file id \"%1\"")
                                .arg(*candidate)});

    if (res == 0) {
      detectedTerminal = QString("open -b %1").arg(*candidate) + " .";
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
