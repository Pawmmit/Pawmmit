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

#include "DiffTool.h"
#include "git/Command.h"
#include "git/Repository.h"
#include "platform/HostProcess.h"
#include "util/Path.h"
#include <QTemporaryFile>
#include <QDebug>

DiffTool::DiffTool(const QString &file, const git::Blob &localBlob,
                   const git::Blob &remoteBlob, QObject *parent)
    : ExternalTool(file, parent), mLocalBlob(localBlob),
      mRemoteBlob(remoteBlob) {}

DiffTool::DiffTool(const QString &file, const git::Blob &localBlob,
                   QObject *parent)
    : ExternalTool(file, parent), mLocalBlob(localBlob) {
  Q_ASSERT(!mRemoteBlob);
}

ExternalTool::Kind DiffTool::kind() const { return Diff; }

QString DiffTool::name() const {
  return mRemoteBlob ? tr("External Diff")
                     : tr("External Diff to Working Copy");
}

bool DiffTool::start() {
  Q_ASSERT(isValid());

  bool shell = false;
  QString command = lookupCommand("diff", shell);
  if (command.isEmpty())
    return false;

  // Write temporary files.
  QString templatePath = QDir::temp().filePath(QFileInfo(mFile).fileName());
  QTemporaryFile *local = nullptr;
  if (mLocalBlob.isValid()) {
    local = new QTemporaryFile(templatePath, this);
    if (!local->open())
      return false;

    local->write(mLocalBlob.content());
    local->flush();
  }

  QString remotePath;
  if (!mRemoteBlob.isValid()) {
    remotePath = mFile;
  } else {
    QTemporaryFile *remote = new QTemporaryFile(templatePath, this);
    if (!remote->open())
      return false;

    remote->write(mRemoteBlob.content());
    remote->flush();

    remotePath = remote->fileName();
  }

  // Destroy this after process finishes.
  auto *process = new platform::HostProcess(this);
  process->setProcessChannelMode(
      QProcess::ProcessChannelMode::ForwardedChannels);
  auto signal = QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished);
  const QProcess *sender = &process->process();
  QObject::connect(sender, signal, [this, process] {
    qDebug() << "Merge Process Exited!";
    qDebug() << "Stdout: " << process->readAllStandardOutput();
    qDebug() << "Stderr: " << process->readAllStandardError();
    deleteLater();
  });

  QString localPath =
      local ? local->fileName() : QFileInfo(mFile).absoluteFilePath();
  const QString hostMerged = util::sandboxPathToHost(mFile);
  QProcessEnvironment vars;
  vars.insert("LOCAL", util::sandboxPathToHost(localPath));
  vars.insert("REMOTE", util::sandboxPathToHost(remotePath));
  vars.insert("MERGED", hostMerged);
  vars.insert("BASE", hostMerged);
  for (const QString &name : vars.keys())
    process->insertEnvironment(name, vars.value(name));

  if (platform::HostProcess::isSandboxed()) {
    process->start("sh", {"-c", command});
  } else if (QString bash = git::Command::bashPath(); !bash.isEmpty()) {
    process->start(bash, {"-c", command});
  } else if (!shell) {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(vars);
    process->start(git::Command::substitute(env, command));
  } else {
    emit error(BashNotFound);
    return false;
  }

  if (!process->waitForStarted()) {
    qDebug() << "DiffTool starting failed";
    return false;
  }

  // Detach from parent.
  setParent(nullptr);

  return true;
}
