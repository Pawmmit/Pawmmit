//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#ifndef INDEXERPROCESS_H
#define INDEXERPROCESS_H

#include "git/Repository.h"
#include <QObject>
#include <QProcess>

// Runs the background indexer worker for a repository.
class IndexerProcess : public QObject {
  Q_OBJECT

public:
  IndexerProcess(const git::Repository &repo, QObject *parent = nullptr);

  /// @brief Start indexing, or restart once the running indexer finishes
  void start();

  /// @brief Stop the indexer, killing it if it doesn't terminate
  void cancel();

signals:
  void started();
  void finished(bool crashed);

  // The index on disk was updated.
  void indexUpdated();

private:
  git::Repository mRepo;
  QProcess mProcess;
  bool mRestart = false;
};

#endif
