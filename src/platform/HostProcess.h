//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#ifndef PLATFORM_HOSTPROCESS_H
#define PLATFORM_HOSTPROCESS_H

#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

namespace platform {

// Runs a program on the host, wrapped in flatpak-spawn when sandboxed.
class HostProcess : public QObject {
public:
  explicit HostProcess(QObject *parent = nullptr);

  // Converted to a host path when sandboxed.
  void setWorkingDirectory(const QString &dir);

  // Added to the inherited environment; the value is passed through unchanged.
  void insertEnvironment(const QString &name, const QString &value);

  void setProcessChannelMode(QProcess::ProcessChannelMode mode);

  void start(const QString &program, const QStringList &arguments = {});
  bool startDetached(const QString &program, const QStringList &arguments = {});
  bool waitForStarted(int msecs = 30000);

  QByteArray readAllStandardOutput();
  QByteArray readAllStandardError();

  // For signals and state only; changes must go through this class.
  const QProcess &process() const { return mProcess; }

  // Whether programs must be started on the host rather than directly.
  static bool isSandboxed();

  // Look up name on the host's PATH, returning an empty string if not found.
  static QString findExecutable(const QString &name);

private:
  QProcessEnvironment mEnvironment;
  QProcess mProcess;
};

} // namespace platform

#endif
