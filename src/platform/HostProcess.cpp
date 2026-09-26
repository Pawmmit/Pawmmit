//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "HostProcess.h"
#include "util/Path.h"
#include <QStandardPaths>

namespace platform {

#ifdef FLATPAK
static QStringList flatpakArguments(const QString &program,
                                    const QStringList &arguments,
                                    const QString &workDir,
                                    const QProcessEnvironment &env) {
  QStringList args = {"--host"};
  if (!workDir.isEmpty())
    args << "--directory=" + util::sandboxPathToHost(workDir);
  for (const QString &name : env.keys())
    args << "--env=" + name + "=" + env.value(name);
  args << program << arguments;
  return args;
}
#else
static void applyEnvironment(QProcess &process,
                             const QProcessEnvironment &additions) {
  if (additions.isEmpty())
    return;

  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert(additions);
  process.setProcessEnvironment(env);
}
#endif

HostProcess::HostProcess(QObject *parent) : QObject(parent) {}

void HostProcess::setWorkingDirectory(const QString &dir) {
  mProcess.setWorkingDirectory(dir);
}

void HostProcess::insertEnvironment(const QString &name, const QString &value) {
  mEnvironment.insert(name, value);
}

void HostProcess::setProcessChannelMode(QProcess::ProcessChannelMode mode) {
  mProcess.setProcessChannelMode(mode);
}

void HostProcess::start(const QString &program, const QStringList &arguments) {
#ifdef FLATPAK
  mProcess.start("flatpak-spawn",
                 flatpakArguments(program, arguments,
                                  mProcess.workingDirectory(), mEnvironment));
#else
  applyEnvironment(mProcess, mEnvironment);
  mProcess.start(program, arguments);
#endif
}

bool HostProcess::startDetached(const QString &program,
                                const QStringList &arguments) {
  // The static QProcess::startDetached() would ignore the directory and
  // environment set on mProcess.
#ifdef FLATPAK
  mProcess.setProgram("flatpak-spawn");
  mProcess.setArguments(flatpakArguments(
      program, arguments, mProcess.workingDirectory(), mEnvironment));
#else
  applyEnvironment(mProcess, mEnvironment);
  mProcess.setProgram(program);
  mProcess.setArguments(arguments);
#endif
  return mProcess.startDetached();
}

bool HostProcess::waitForStarted(int msecs) {
  return mProcess.waitForStarted(msecs);
}

bool HostProcess::waitForFinished(int msecs) {
  return mProcess.waitForFinished(msecs);
}

QByteArray HostProcess::readAllStandardOutput() {
  return mProcess.readAllStandardOutput();
}

QByteArray HostProcess::readAllStandardError() {
  return mProcess.readAllStandardError();
}

bool HostProcess::isSandboxed() {
#ifdef FLATPAK
  return true;
#else
  return false;
#endif
}

QString HostProcess::findExecutable(const QString &name) {
#ifdef FLATPAK
  QProcess process;
  process.start("flatpak-spawn",
                {"--host", "sh", "-c", "command -v \"$1\"", "sh", name});
  if (!process.waitForFinished(3000) || process.exitCode() != 0)
    return QString();

  return QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed();
#else
  return QStandardPaths::findExecutable(name);
#endif
}
} // namespace platform
