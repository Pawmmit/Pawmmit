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

#include "MergeTool.h"
#include "git/Command.h"
#include "git/Config.h"
#include "git/Index.h"
#include "git/Repository.h"
#include "platform/HostProcess.h"
#include "util/Path.h"
#include "Debug.h"
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryFile>

MergeTool::MergeTool(const QString &file, const git::Blob &localBlob,
                     const git::Blob &remoteBlob, const git::Blob &baseBlob,
                     QObject *parent)
    : ExternalTool(file, parent), mLocalBlob(localBlob),
      mRemoteBlob(remoteBlob), mBaseBlob(baseBlob) {}

bool MergeTool::isValid() const {
  return (ExternalTool::isValid() && mLocalBlob.isValid() &&
          mRemoteBlob.isValid());
}

ExternalTool::Kind MergeTool::kind() const { return Merge; }

QString MergeTool::name() const { return tr("External Merge"); }

bool MergeTool::start() {
  Q_ASSERT(isValid());

  bool shell = false;
  QString command = lookupCommand("merge", shell);
  if (command.isEmpty())
    return false;

  // Write temporary files.
  QString templatePath = QDir::temp().filePath(QFileInfo(mFile).fileName());
  QTemporaryFile *local = new QTemporaryFile(templatePath, this);
  if (!local->open())
    return false;

  local->write(mLocalBlob.content());
  local->flush();

  QTemporaryFile *remote = new QTemporaryFile(templatePath, this);
  if (!remote->open())
    return false;

  remote->write(mRemoteBlob.content());
  remote->flush();

  QString basePath;
  if (mBaseBlob.isValid()) {
    QTemporaryFile *base = new QTemporaryFile(templatePath, this);
    if (!base->open())
      return false;

    base->write(mBaseBlob.content());
    base->flush();

    basePath = base->fileName();
  }

  // Make the backup copy.
  QString backupPath = QString("%1.orig").arg(mFile);
  if (!QFile::copy(mFile, backupPath)) {
    // FIXME: What should happen if the backup already exists?
  }

  // Destroy this after process finishes.
  auto *process = new platform::HostProcess(this);
  process->setProcessChannelMode(
      QProcess::ProcessChannelMode::ForwardedChannels);
  git::Repository repo = mLocalBlob.repo();
  auto signal = QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished);
  const QProcess *sender = &process->process();
  QObject::connect(sender, signal, [this, repo, backupPath, process] {
    Debug("Merge Process Exited!");
    Debug("Stdout: " << process->readAllStandardOutput());
    Debug("Stderr: " << process->readAllStandardError());

    QFileInfo merged(mFile);
    QFileInfo backup(backupPath);
    git::Config config = git::Config::global();
    bool modified = (merged.lastModified() > backup.lastModified());
    if (!modified || !config.value<bool>("mergetool.keepBackup"))
      QFile::remove(backupPath);

    if (modified) {
      int length = repo.workdir().path().length();
      repo.index().setStaged({mFile.mid(length + 1)}, true);
    }

    deleteLater();
  });

  QProcessEnvironment vars;
  vars.insert("LOCAL", local->fileName());
  vars.insert("REMOTE", remote->fileName());
  vars.insert("MERGED", util::sandboxPathToHost(mFile));
  vars.insert("BASE", basePath);
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
    Debug("MergeTool starting failed");
    return false;
  }

  // Detach from parent.
  setParent(nullptr);

  return true;
}
