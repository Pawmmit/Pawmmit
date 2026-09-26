//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "Terminal.h"
#include <QDir>
#include <QFile>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QStringList>
#include <Windows.h>
#include <memory>

namespace platform {

QString defaultTerminalCommand() {
  static QString detectedTerminal = nullptr;
  if (!detectedTerminal.isNull())
    return detectedTerminal;

  detectedTerminal = "";

  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  QString programFilesDir = env.value("PROGRAMFILES");
  QString programFiles32Dir = env.value("PROGRAMFILES(x86)");

  QStringList candidates;

  candidates.append("git-bash");
  if (!programFilesDir.isEmpty())
    candidates.append(programFilesDir + "/Git/git-bash.exe");
  if (!programFiles32Dir.isEmpty())
    candidates.append(programFiles32Dir + "/Git/git-bash.exe");
  if (!programFilesDir.isEmpty())
    candidates.append(programFilesDir + "/Git/bin/bash.exe");
  if (!programFiles32Dir.isEmpty())
    candidates.append(programFiles32Dir + "/Git/bin/bash.exe");
  candidates.append("cmd");

  for (QString candidate : candidates) {
    QString exePath;

    if (QDir::isAbsolutePath(candidate)) {
      if (QFile::exists(candidate))
        exePath = candidate;

    } else {
      exePath = QStandardPaths::findExecutable(candidate);
    }

    if (!exePath.isEmpty()) {
      detectedTerminal =
          '"' + QDir::toNativeSeparators(exePath.replace("\"", "\"\"")) + '"';
      break;
    }
  }

  return detectedTerminal;
}

bool openTerminal(const QString &command, const QString &dir) {
  // No direct method of QProcess can take a raw command line and a working
  // directory So we call CreateProcessW() directly

  std::unique_ptr<wchar_t[]> cmdBuffer(new wchar_t[command.length() + 1]);
  int len = command.toWCharArray(cmdBuffer.get());
  cmdBuffer[len] = L'\0';

  STARTUPINFOW startupInfo;
  PROCESS_INFORMATION processInfo;

  ZeroMemory(&startupInfo, sizeof(STARTUPINFOW));
  ZeroMemory(&processInfo, sizeof(PROCESS_INFORMATION));

  bool success = CreateProcessW(nullptr, cmdBuffer.get(), nullptr, nullptr,
                                FALSE, CREATE_NEW_CONSOLE, nullptr,
                                (LPCWSTR)QDir::toNativeSeparators(dir).utf16(),
                                &startupInfo, &processInfo);

  if (!success)
    return false;

  CloseHandle(processInfo.hProcess);
  CloseHandle(processInfo.hThread);
  return true;
}

} // namespace platform
