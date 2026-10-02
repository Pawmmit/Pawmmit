//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "CommitModel.h"
#include "ConfigKeys.h"
#include "Debug.h"
#include "app/Application.h"
#include "conf/Settings.h"
#include "git/Branch.h"
#include "git/Config.h"
#include "git/Index.h"
#include <QWidget>
#include <QtConcurrent>

using namespace graph;

CommitModel::CommitModel(const git::Repository &repo, QObject *parent)
    : QAbstractListModel(parent), mRepo(repo) {
  // Connect progress timer.
  connect(&mTimer, &QTimer::timeout, [this] {
    ++mProgress;
    QModelIndex idx = index(0, 0);
    emit dataChanged(idx, idx, {Qt::DisplayRole});
  });

  // Connect watcher to signal when the status diff finishes.
  connect(&mStatus, &QFutureWatcher<git::Diff>::finished, [this] {
    mTimer.stop();
    mMerging = (mRepo.operation() == git::Operation::Merge);
    dispatchResetWalker(true);
  });

  // Apply the result of an asynchronous walker reset on the GUI thread.
  connect(&mReset, &QFutureWatcher<ResetResult>::finished, [this] {
    ResetResult result = mReset.result();
    bool emitStatusFinished = result.emitStatusFinished;
    applyResetResult(std::move(result));
    if (emitStatusFinished)
      emit statusFinished(!mRows.isEmpty() && !mRows.first().commit.isValid());
  });

  resetSettings();
}

CommitModel::~CommitModel() {
  // Ensure that mStatus is stopped since it captures `this` and potentially
  // might crash after the destructor is finished
  cancelStatus();

  // ..and the same applies to mReset too
  if (mReset.isRunning())
    mReset.waitForFinished();
}

git::Diff CommitModel::status() const {
  if (!mStatus.isFinished())
    return git::Diff();

  QFuture<git::Diff> future = mStatus.future();
  if (!future.resultCount())
    return git::Diff();

  return future.result();
}

void CommitModel::startStatus() {
  // Cancel existing status diff.
  cancelStatus();

  // Reload the index before starting the status thread. Allowing
  // it to reload on the thread frequently corrupts the index.
  mRepo.index().read();

  // Check for uncommitted changes asynchronously.
  emit loadingChanged(true);
  mProgress = 0;
  mTimer.start(50);
  mStatus.setFuture(QtConcurrent::run([this] {
    // Pass the repo's index to suppress reload.
    bool ignoreWhitespace = Settings::instance()->isWhitespaceIgnored();
    return mRepo.status(mRepo.index(), &mStatusCallbacks, ignoreWhitespace);
  }));
}

void CommitModel::cancelStatus() {
  if (!mStatus.isRunning())
    return;

  mStatusCallbacks.setCanceled(true);
  mStatus.waitForFinished();
  mStatus.setFuture(QFuture<git::Diff>());
  mStatusCallbacks.setCanceled(false);
}

void CommitModel::setPathspec(const QString &pathspec) {
  if (mPathspec == pathspec)
    return;

  mPathspec = pathspec;
  resetWalker();
}

void CommitModel::setReference(const git::Reference &ref) {
  mRef = ref;
  if (!mSuppressResetWalker) {
    resetWalker();
  }
}

void CommitModel::resetReference(const git::Reference &ref) {
  // Reset selected ref to updated ref.
  if (ref.isValid() && mRef.isValid() &&
      ref.qualifiedName() == mRef.qualifiedName())
    mRef = ref;

  // Status is invalid after HEAD changes.
  if (!ref.isValid() || ref.isHead())
    startStatus();
  else if (!mSuppressResetWalker) {
    // reset walker will be done when status finished
    resetWalker();
  }
}

void CommitModel::resetSettings(bool walk) {
  git::Config config = mRepo.appConfig();
  mRefsFilter = static_cast<CommitList::RefsFilter>(config.value<int>(
      ConfigKeys::kRefsKey, (int)CommitList::RefsFilter::AllRefs));
  mSortDate = config.value<bool>(ConfigKeys::kSortKey, true);
  mShowCleanStatus = config.value<bool>(ConfigKeys::kStatusKey, true);
  mGraphVisible = config.value<bool>(ConfigKeys::kGraphKey, true);

  if (walk)
    resetWalker();
}

bool CommitModel::canFetchMore(const QModelIndex &parent) const {
  return mWalker.isValid();
}

void CommitModel::fetchMore(const QModelIndex &parent) {
  FetchResult fetched =
      fetchRows(mWalker, mParents, mRows, mPathspec, mGraphVisible,
                mRefsFilter == CommitList::RefsFilter::SelectedRefIgnoreMerge,
                Application::theme()->branchTopologyEdges());

  // Update the model.
  if (!fetched.rows.isEmpty()) {
    int first = mRows.size();
    int last = first + fetched.rows.size() - 1;
    beginInsertRows(QModelIndex(), first, last);
    mRows.append(fetched.rows);
    endInsertRows();
  }

  // Invalidate walker.
  if (fetched.exhausted)
    mWalker = git::RevWalk();
}

int CommitModel::rowCount(const QModelIndex &parent) const {
  return mRows.size();
}

QVariant CommitModel::data(const QModelIndex &index, int role) const {
  if (index.row() >= mRows.size())
    return QVariant();
  const Row &row = mRows.at(index.row());
  bool status = !row.commit.isValid();
  switch (role) {
    case Qt::DisplayRole:
      if (!status)
        return QVariant();

      if (!mStatus.isFinished())
        return tr("Checking for uncommitted changes");

      // During a merge this row is where it's committed.
      if (mMerging) {
        git::Diff diff = this->status();
        return (diff.isValid() && diff.isConflicted())
                   ? tr("Merge in progress")
                   : tr("Merge ready to commit");
      }

      return tr("Uncommitted changes");

    case Qt::FontRole: {
      if (!status)
        return QVariant();

      QFont font = static_cast<QWidget *>(QObject::parent())->font();
      font.setItalic(true);
      return font;
    }

    case Qt::TextAlignmentRole:
      if (!status)
        return QVariant();

      return QVariant(Qt::AlignHCenter | Qt::AlignVCenter);

    case Qt::DecorationRole:
      if (!status)
        return QVariant();

      return mStatus.isFinished() ? QVariant() : mProgress;

    case CommitList::Role::DiffRole: {
      if (status)
        return QVariant::fromValue(this->status());

      bool ignoreWhitespace = Settings::instance()->isWhitespaceIgnored();
      git::Diff diff = row.commit.diff(git::Commit(), -1, ignoreWhitespace);
      diff.findSimilar();
      return QVariant::fromValue(diff);
    }

    case CommitList::Role::CommitRole:
      return status ? QVariant() : QVariant::fromValue(row.commit);

    case CommitList::Role::GraphRole: {
      QVariantList columns;
      for (const Column &column : row.columns) {
        QVariantList segments;
        for (const Segment &segment : column)
          segments.append(segment.segment);
        columns.append(QVariant(segments));
      }

      return columns;
    }

    case CommitList::Role::GraphColorRole: {
      QVariantList columns;
      for (const Column &column : row.columns) {
        QVariantList segments;
        for (const Segment &segment : column)
          segments.append(segment.color);
        columns.append(QVariant(segments));
      }

      return columns;
    }
  }

  return QVariant();
}

CommitModel::ResetResult CommitModel::computeReset(const ResetContext &ctx) {
  ResetResult result;

  // Update status row.
  bool head = (!ctx.ref.isValid() || ctx.ref.isHead());
  bool valid = (!ctx.statusCheckFinished || ctx.statusDiff.isValid());
  if (ctx.showCleanStatus && head && valid && ctx.pathspec.isEmpty()) {
    QVector<Column> row;
    if (ctx.graphVisible && ctx.ref.isValid() && ctx.statusCheckFinished) {
      row.append({Segment(Bottom, kTaintedColor), Segment(Dot, QColor())});
      result.parents.append(Parent(
          ctx.ref.target(), nextColor(result.parents, ctx.colors), true));
    }
    result.rows.append(Row(git::Commit(), row)); // Uncommitted changes
  }

  // Begin walking commits.
  if (ctx.ref.isValid()) {
    int sort = GIT_SORT_NONE;
    if (ctx.graphVisible) {
      sort |= GIT_SORT_TOPOLOGICAL;
      if (ctx.sortDate)
        sort |= GIT_SORT_TIME;
    } else if (!ctx.sortDate) {
      sort |= GIT_SORT_TOPOLOGICAL;
    }

    result.walker = ctx.ref.walker(
        sort, ctx.refsFilter == CommitList::RefsFilter::SelectedRefIgnoreMerge);
    if (ctx.ref.isLocalBranch()) {
      // Add the upstream branch.
      if (git::Branch upstream = git::Branch(ctx.ref).upstream())
        result.walker.push(upstream);
    }

    if (ctx.ref.isHead()) {
      // Add merge head.
      if (git::Reference mergeHead = ctx.repo.lookupRef("MERGE_HEAD"))
        result.walker.push(mergeHead);
    }

    if (ctx.refsFilter == CommitList::RefsFilter::AllRefs) {
      for (const git::Reference &ref : ctx.repo.refs()) {
        if (!ref.isStash())
          result.walker.push(ref);
      }
    }
  }

  if (result.walker.isValid()) {
    FetchResult fetched = fetchRows(
        result.walker, result.parents, result.rows, ctx.pathspec,
        ctx.graphVisible,
        ctx.refsFilter == CommitList::RefsFilter::SelectedRefIgnoreMerge,
        ctx.colors);
    result.rows.append(fetched.rows);
    if (fetched.exhausted)
      result.walker = git::RevWalk();
  }

  result.emitStatusFinished = ctx.emitStatusFinished;
  return result;
}

void CommitModel::dispatchResetWalker(bool emitStatusFinishedAfter) {
  ResetContext ctx{mRef,
                   mPathspec,
                   mGraphVisible,
                   mSortDate,
                   mRefsFilter,
                   mShowCleanStatus,
                   mRepo,
                   status(),
                   mStatus.isFinished(),
                   Application::theme()->branchTopologyEdges(),
                   emitStatusFinishedAfter};

  emit loadingChanged(true);
  mReset.setFuture(QtConcurrent::run([ctx] { return computeReset(ctx); }));
}

void CommitModel::applyResetResult(ResetResult &&result) {
  beginResetModel();
  mParents = std::move(result.parents);
  mRows = std::move(result.rows);
  mWalker = std::move(result.walker);
  DebugRefresh("");
  endResetModel();
  emit loadingChanged(false);
}

void ListModel::setList(const QList<git::Commit> &commits) {
  beginResetModel();
  mCommits = commits;
  endResetModel();
}

int ListModel::rowCount(const QModelIndex &parent) const {
  return mCommits.size();
}

QVariant ListModel::data(const QModelIndex &index, int role) const {
  switch (role) {
    case CommitList::Role::DiffRole: {
      git::Commit commit = mCommits.at(index.row());
      bool ignoreWhitespace = Settings::instance()->isWhitespaceIgnored();
      git::Diff diff = commit.diff(git::Commit(), -1, ignoreWhitespace);
      diff.findSimilar();
      return QVariant::fromValue(diff);
    }

    case CommitList::Role::CommitRole:
      return QVariant::fromValue(mCommits.at(index.row()));
  }

  return QVariant();
}
