//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "FileManager.h"
#include "HostProcess.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusUnixFileDescriptor>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QVariantMap>
#include <fcntl.h>
#include <unistd.h>

namespace platform {

namespace {

const int kTimeout = 5000;

// Asks the desktop's file manager to open the folder with the file selected.
bool showItem(const QString &file) {
  QDBusMessage msg = QDBusMessage::createMethodCall(
      "org.freedesktop.FileManager1", "/org/freedesktop/FileManager1",
      "org.freedesktop.FileManager1", "ShowItems");
  msg << QStringList(QUrl::fromLocalFile(file).toString()) << QString();

  QDBusMessage reply =
      QDBusConnection::sessionBus().call(msg, QDBus::Block, kTimeout);
  return reply.type() == QDBusMessage::ReplyMessage;
}

// The portal equivalent of showItem() for use inside a sandbox.
bool openDirectoryPortal(const QString &file) {
  int fd = ::open(QFile::encodeName(file).constData(), O_PATH | O_CLOEXEC);
  if (fd < 0)
    return false;

  QDBusUnixFileDescriptor descriptor(fd);
  ::close(fd);

  QDBusMessage msg = QDBusMessage::createMethodCall(
      "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
      "org.freedesktop.portal.OpenURI", "OpenDirectory");
  msg << QString() << QVariant::fromValue(descriptor) << QVariantMap();

  QDBusMessage reply =
      QDBusConnection::sessionBus().call(msg, QDBus::Block, kTimeout);
  return reply.type() == QDBusMessage::ReplyMessage;
}

} // namespace

QString defaultFileManagerCommand() { return "xdg-open \"%1\""; }

bool revealInFileManager(const QString &file, const QString &command) {
  if (command.isEmpty()) {
    bool shown =
        HostProcess::isSandboxed() ? openDirectoryPortal(file) : showItem(file);
    if (shown)
      return true;
  }

  // Without a way to select the file, open its directory instead.
  QFileInfo info(file);
  QString dir = info.isDir() ? info.filePath() : info.path();
  return openFileManager(
      !command.isEmpty() ? command : defaultFileManagerCommand(), dir);
}

} // namespace platform
