//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "IndexerProcess.h"
#include "Index.h"
#include "Debug.h"
#include "git/Config.h"
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>

IndexerProcess::IndexerProcess(const git::Repository &repo, QObject *parent)
    : QObject(parent), mRepo(repo) {
  connect(&mProcess, &QProcess::started, this, &IndexerProcess::started);

  using Signal = void (QProcess::*)(int, QProcess::ExitStatus);
  auto signal = static_cast<Signal>(&QProcess::finished);
  connect(&mProcess, signal, this,
          [this](int code, QProcess::ExitStatus status) {
            Q_UNUSED(code)

            emit finished(status == QProcess::CrashExit);

            if (mRestart) {
              mRestart = false;
              start();
            }
          });

  // Forward indexer stderr. Read from stdout.
  mProcess.setProcessChannelMode(QProcess::ForwardedErrorChannel);
  connect(&mProcess, &QProcess::readyReadStandardOutput, this, [this] {
    mProcess.readAllStandardOutput();
    emit indexUpdated();
  });
}

void IndexerProcess::start() {
  if (!mRepo.appConfig().value<bool>("index.enable", true))
    return;

  if (mProcess.state() != QProcess::NotRunning) {
    mRestart = true;
    return;
  }

  QStringList args = {"--notify", "--background", mRepo.dir().path()};
  if (Index::isLoggingEnabled())
    args.prepend("--log");

  QDir dir(QCoreApplication::applicationDirPath());
  QString indexer =
      QStandardPaths::findExecutable("pawmmit-indexer", {dir.path()});
  if (indexer.isEmpty()) {
    indexer = dir.filePath("pawmmit-indexer");
    Debug("No indexer found: " << indexer);
  }

  mProcess.start(indexer, args);
}

void IndexerProcess::cancel() {
  if (mProcess.state() == QProcess::NotRunning)
    return;

  mProcess.terminate();
  mProcess.waitForFinished(5000);

  if (mProcess.state() == QProcess::NotRunning)
    return;

  mProcess.kill();
  mProcess.waitForFinished(5000);
}
