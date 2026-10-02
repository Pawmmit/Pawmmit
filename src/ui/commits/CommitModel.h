//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#ifndef COMMITMODEL_H
#define COMMITMODEL_H

#include "CommitGraph.h"
#include "CommitList.h"
#include "git/Diff.h"
#include "git/Reference.h"
#include "git/Repository.h"
#include "git/RevWalk.h"
#include <QAbstractListModel>
#include <QFutureWatcher>
#include <QTimer>

/*!
 * \brief The CommitModel class
 * Model showing all commits as timeline
 */
class CommitModel : public QAbstractListModel {
  Q_OBJECT

public:
  CommitModel(const git::Repository &repo, QObject *parent = nullptr);
  ~CommitModel();

  git::Reference reference() const { return mRef; }

  git::Diff status() const;
  void startStatus();
  void cancelStatus();

  void setPathspec(const QString &pathspec);

  void suppressResetWalker(bool suppress) { mSuppressResetWalker = suppress; }
  bool isResetWalkerSuppressed() { return mSuppressResetWalker; }

  void setReference(const git::Reference &ref);
  void resetReference(const git::Reference &ref);

  // Rebuild the walker and the first page of rows. The expensive part
  // (building the revwalk over all refs and computing the graph for the
  // first page of commits) runs on a background thread; see
  // dispatchResetWalker().
  void resetWalker() { dispatchResetWalker(false); }

  void resetSettings(bool walk = false);

  bool canFetchMore(const QModelIndex &parent) const override;
  void fetchMore(const QModelIndex &parent) override;

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;

signals:
  void statusFinished(bool visible);
  void loadingChanged(bool loading);

private:
  class DiffCallbacks : public git::Diff::Callbacks {
  public:
    void setCanceled(bool canceled) { mCanceled = canceled; }

    bool progress(const QString &oldPath, const QString &newPath) override {
      return !mCanceled;
    }

  private:
    bool mCanceled = false;
  };

  // Everything the background thread needs to rebuild the walker and the
  // first page of rows. Captured by value at dispatch time so the
  // computation can run on another thread without touching model state.
  struct ResetContext {
    git::Reference ref;
    QString pathspec;
    bool graphVisible;
    bool sortDate;
    CommitList::RefsFilter refsFilter;
    bool showCleanStatus;
    git::Repository repo;
    git::Diff statusDiff;
    bool statusCheckFinished;
    QList<QColor> colors;

    // Carried straight through to ResetResult; see its field for why.
    bool emitStatusFinished;
  };

  struct ResetResult {
    QList<graph::Parent> parents;
    QList<graph::Row> rows;
    git::RevWalk walker;

    // Whether this particular reset was triggered by the status check
    // finishing, and should therefore emit statusFinished() once applied.
    bool emitStatusFinished = false;
  };

  // Build the walker and the first page of rows. Safe to run off the GUI
  // thread: it only touches the context passed in and returns a fresh
  // result rather than mutating model state directly.
  static ResetResult computeReset(const ResetContext &ctx);

  // Kick off an asynchronous walker reset. The GUI thread keeps showing the
  // previous rows (behind a loading indicator, see CommitList::setLoading)
  // until the background computation finishes and applyResetResult() swaps
  // the new data in.
  void dispatchResetWalker(bool emitStatusFinishedAfter);

  // Apply a completed background reset on the GUI thread.
  void applyResetResult(ResetResult &&result);

  QTimer mTimer;
  int mProgress = 0;

  DiffCallbacks mStatusCallbacks;
  QFutureWatcher<git::Diff> mStatus;

  QFutureWatcher<ResetResult> mReset;

  QString mPathspec;
  git::Reference mRef;
  git::RevWalk mWalker;
  git::Repository mRepo;

  QList<graph::Row> mRows;
  QList<graph::Parent> mParents;

  // walker settings
  bool mSuppressResetWalker{false};
  CommitList::RefsFilter mRefsFilter{CommitList::RefsFilter::AllRefs};
  bool mSortDate = true;
  bool mShowCleanStatus = true;
  bool mMerging = false;
  bool mGraphVisible = true;
};

/*!
 * \brief The ListModel class
 * Used to show a list of commits. This is used when a filter is used
 */
class ListModel : public QAbstractListModel {
public:
  ListModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}

  void setList(const QList<git::Commit> &commits);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;

private:
  QList<git::Commit> mCommits;
};

#endif
