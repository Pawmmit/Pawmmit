//
//          Copyright (c) 2016, Scientific Toolworks, Inc.
//
// This software is licensed under the GNU General Public License v3.0 or
// (at your option) any later version. The LICENSE.md file describes the
// conditions under which this software may be distributed.
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Author: Jason Haslam
//

#include "CommitList.h"
#include "CommitContextMenu.h"
#include "CommitDelegate.h"
#include "CommitModel.h"
#include "Debug.h"
#include "app/Application.h"
#include "conf/Settings.h"
#include "git/Branch.h"
#include "git/Commit.h"
#include "git/Config.h"
#include "git/Diff.h"
#include "git/Index.h"
#include "git/Patch.h"
#include "git/Signature.h"
#include "git/TagRef.h"
#include "git/Tree.h"
#include "index/Index.h"
#include "ui/ProgressIndicator.h"
#include "ui/hotkeys/HotkeyManager.h"
#include "ui/repo/Location.h"
#include "ui/repo/RepoView.h"
#include "ui/window/MainWindow.h"
#include <QApplication>
#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QtConcurrent>

namespace {

const QString kPathspecFmt = "pathspec:%1";

class SelectionModel : public QItemSelectionModel {
public:
  SelectionModel(QAbstractItemModel *model) : QItemSelectionModel(model) {}

  void select(const QItemSelection &selection,
              QItemSelectionModel::SelectionFlags command) {
    if ((command == QItemSelectionModel::Select ||
         command == QItemSelectionModel::SelectCurrent ||
         command == (QItemSelectionModel::Current |
                     QItemSelectionModel::ClearAndSelect)) &&
        (selectedIndexes().size() >= 2 || selection.indexes().size() > 1))
      return;

    QItemSelectionModel::select(selection, command);
  }
};

} // namespace

static Hotkey selectCommitDownHotKey = HotkeyManager::registerHotkey(
    "j", "commitList/selectCommitDown", "CommitList/Select Next Commit Down");

static Hotkey selectCommitUpHotKey = HotkeyManager::registerHotkey(
    "k", "commitList/selectCommitUp", "CommitList/Select Next Commit Up");

CommitList::CommitList(Index *index, QWidget *parent)
    : QListView(parent), mIndex(index) {
  Theme *theme = Application::theme();
  setPalette(theme->commitList());

  git::Repository repo = index->repo();
  mList = new ListModel(this);
  mModel = new CommitModel(repo, this);

  connect(&mTimer, &QTimer::timeout, this, [this] {
    ++mProgress;
    if (mLoadingFadein < 1.0f)
      mLoadingFadein += 0.1;
    viewport()->update();
  });

  setMouseTracking(true);
  setUniformItemSizes(true);
  setAttribute(Qt::WA_MacShowFocusRect, false);
  setSelectionMode(QAbstractItemView::ExtendedSelection);

  setModel(mModel);
  setItemDelegate(new CommitDelegate(repo, this));

  connect(mModel, &QAbstractItemModel::modelAboutToBeReset, this,
          &CommitList::storeSelection);
  connect(mModel, &QAbstractItemModel::modelReset, this,
          &CommitList::restoreSelection);
  connect(mList, &QAbstractItemModel::modelAboutToBeReset, this,
          &CommitList::storeSelection);
  connect(mList, &QAbstractItemModel::modelReset, this,
          &CommitList::restoreSelection);

  CommitModel *model = static_cast<CommitModel *>(mModel);
  connect(model, &CommitModel::statusFinished, [this](bool visible) {
    mRestoreSelection = true; // Reset to default

    // Select the first commit if the selection was cleared.
    if (selectedIndexes().isEmpty())
      selectFirstCommit();

    // Notify main window.
    emit statusChanged(visible);
  });

  connect(model, &CommitModel::loadingChanged, this, &CommitList::setLoading);

  git::RepositoryNotifier *notifier = repo.notifier();
  connect(notifier, &git::RepositoryNotifier::referenceUpdated,
          [this](const git::Reference &ref, bool restoreSelection) {
            mRestoreSelection = restoreSelection;
            resetReference(ref);
          });
  connect(notifier, &git::RepositoryNotifier::workdirChanged, [this] {
    resetReference(static_cast<const CommitModel *>(mModel)->reference());
  });

  connect(this, &CommitList::entered,
          [this](const QModelIndex &index) { update(index); });

  QShortcut *shortcut = new QShortcut(this);
  selectCommitDownHotKey.use(shortcut);
  connect(shortcut, &QShortcut::activated, [this] { selectCommitRelative(1); });

  shortcut = new QShortcut(this);
  selectCommitUpHotKey.use(shortcut);
  connect(shortcut, &QShortcut::activated,
          [this] { selectCommitRelative(-1); });

#ifdef Q_OS_MAC
  QFont font = this->font();
  font.setPointSize(13);
  setFont(font);
#endif
}

git::Diff CommitList::status() const {
  return static_cast<CommitModel *>(mModel)->status();
}

QString CommitList::selectedRange() const {
  QList<git::Commit> commits = selectedCommits();
  if (commits.isEmpty())
    return !selectedIndexes().isEmpty() ? "status" : QString();

  git::Commit first = commits.first();
  if (commits.size() == 1)
    return first.id().toString();

  git::Commit last = commits.last();
  return QString("%1..%2").arg(last.id().toString(), first.id().toString());
}

git::Diff CommitList::selectedDiff() const {
  QModelIndexList indexes = sortedIndexes();
  DebugRefresh("Selected indices count: " << indexes.count());
  for (const auto &index : indexes) {
    const auto &id = index.data(CommitRole).value<git::Commit>().shortId();
    (void)id; // Unused in release builds
    DebugRefresh("Commit: " << id);
  }
  if (indexes.isEmpty())
    return git::Diff();

  if (indexes.size() == 1) {
    auto first = indexes.first().data(DiffRole);
    return first.isValid() ? first.value<git::Diff>() : git::Diff();
  }

  git::Commit first = indexes.first().data(CommitRole).value<git::Commit>();
  if (!first.isValid())
    return git::Diff();

  git::Commit last = indexes.last().data(CommitRole).value<git::Commit>();
  bool ignoreWhitespace = Settings::instance()->isWhitespaceIgnored();
  git::Diff diff = first.diff(last, -1, ignoreWhitespace);
  diff.findSimilar();
  return diff;
}

QList<git::Commit> CommitList::selectedCommits() const {
  QList<git::Commit> selectedCommits;
  for (const QModelIndex &index : sortedIndexes()) {
    git::Commit commit = index.data(CommitRole).value<git::Commit>();
    if (commit.isValid())
      selectedCommits.append(commit);
  }

  return selectedCommits;
}

void CommitList::cancelStatus() {
  static_cast<CommitModel *>(mModel)->cancelStatus();
}

void CommitList::setReference(const git::Reference &ref) {
  auto *model = static_cast<CommitModel *>(mModel);
  git::Reference previous = model->reference();
  model->setReference(ref);
  if (!isResetWalkerSuppressed())
    updateModel();

  // Only steal focus for a real branch/tag switch, not a passive resync to
  // the same reference (e.g. on every background refresh).
  bool changed =
      previous.isValid() != ref.isValid() ||
      (ref.isValid() && previous.qualifiedName() != ref.qualifiedName());
  if (changed)
    setFocus();
}

void CommitList::setFilter(const QString &filter) {
  mFilter = filter.simplified();
  updateModel();
}

void CommitList::setPathspec(const QString &pathspec, bool index) {
  if (index) {
    setFilter(!pathspec.isEmpty() ? kPathspecFmt.arg(pathspec) : QString());
  } else {
    static_cast<CommitModel *>(mModel)->setPathspec(pathspec);
  }
}

void CommitList::setCommits(const QList<git::Commit> &commits) {
  setModel(mList);
  static_cast<ListModel *>(mList)->setList(commits);
}

void CommitList::selectReference(const git::Reference &ref) {
  if (!ref.isValid())
    return;

  QModelIndex index = model()->index(0, 0);
  if (ref.isHead() && !index.data(CommitRole).isValid()) {
    selectFirstCommit();
  } else {
    selectRange(ref.target().id().toString());
  }
}

void CommitList::resetSelection(bool spontaneous) {
  // Just notify.
  mSpontaneous = spontaneous;
  notifySelectionChanged();
  mSpontaneous = true;
}

void CommitList::selectFirstCommit(bool spontaneous) {
  QModelIndex index = model()->index(0, 0);
  const auto commit = index.data(CommitRole).value<git::Commit>();
  if (commit.isValid())
    DebugRefresh("Commit id: " << commit.shortId());
  else
    DebugRefresh("Invalid commit");
  if (index.isValid()) {
    selectIndexes(QItemSelection(index, index), QString(), spontaneous);
  } else {
    // Invalidate any in-flight async diff so a stale result for a
    // previously selected commit can't be delivered after this reset.
    ++mDiffRequest;
    emit diffSelected(git::Diff());
  }

  // This is the automatic fallback selection, not a deliberate pick, so a
  // later background refresh is free to move it instead of pinning it here.
  mSelectionIsDefault = true;
}

bool CommitList::selectStatus(const QString &file) {
  QModelIndex index = model()->index(0, 0);
  if (!index.isValid() || index.data(CommitRole).isValid())
    return false;

  // Reselecting the same row emits no selection change, so dispatch directly.
  if (selectionModel()->selectedIndexes() == QModelIndexList{index}) {
    dispatchSelectedDiff(file, false);
    scrollTo(index);
    return true;
  }

  selectIndexes(QItemSelection(index, index), file, false);
  return true;
}

void CommitList::selectCommitRelative(int offset) {
  QModelIndexList indices = selectionModel()->selectedIndexes();
  QModelIndex index = indices[0];
  if (!index.isValid()) {
    return;
  }
  QModelIndex new_index = model()->index(index.row() + offset, index.column());
  if (!new_index.isValid()) {
    return;
  }
  selectIndexes(QItemSelection(new_index, new_index), QString(), true);
}

bool CommitList::selectRange(const QString &range, const QString &file,
                             bool spontaneous, bool dispatchDiff) {
  // Try to select the "status" index.
  QModelIndex index = model()->index(0, 0);
  if (range == "status" && !index.data(CommitRole).isValid()) {
    return true;
  }

  QStringList ids = range.split("..");
  if (ids.size() > 2)
    return false;

  // Invert range.
  bool one = (ids.size() == 1);
  git::Repository repo = RepoView::parentView(this)->repo();
  git::Commit firstCommit = repo.lookupCommit(ids.last());
  git::Commit lastCommit = one ? firstCommit : repo.lookupCommit(ids.first());

  // Check for already selected range.
  QModelIndexList indexes = sortedIndexes();
  if (indexes.size() >= 2) {
    git::Commit first = indexes.first().data(CommitRole).value<git::Commit>();
    git::Commit last = indexes.last().data(CommitRole).value<git::Commit>();
    if (first.isValid() && first == firstCommit && last.isValid() &&
        last == lastCommit)
      return false;
  }

  // Find indexes.
  QItemSelection selection;
  QModelIndex first = findCommit(firstCommit);
  if (!first.isValid())
    return false;
  selection.select(first, first);

  if (lastCommit != firstCommit) {
    QModelIndex last = findCommit(lastCommit);
    if (!last.isValid())
      return false;
    selection.select(last, last);
  }

  selectIndexes(selection, file, spontaneous, dispatchDiff);
  return true;
}

void CommitList::suppressResetWalker(bool suppress) {
  static_cast<CommitModel *>(mModel)->suppressResetWalker(suppress);
}

void CommitList::resetReference(const git::Reference &ref) {
  static_cast<CommitModel *>(mModel)->resetReference(ref);
}

bool CommitList::isResetWalkerSuppressed() {
  return static_cast<CommitModel *>(mModel)->isResetWalkerSuppressed();
}

void CommitList::resetSettings() {
  static_cast<CommitModel *>(mModel)->resetSettings(true);
}

void CommitList::setModel(QAbstractItemModel *model) {
  if (model == this->model())
    return;

  storeSelection();

  // Destroy the previous selection model.
  delete selectionModel();

  QListView::setModel(model);

  // Destroy the selection model created by Qt.
  delete selectionModel();

  SelectionModel *selectionModel = new SelectionModel(model);
  connect(
      selectionModel, &QItemSelectionModel::selectionChanged,
      [this](const QItemSelection &selected, const QItemSelection &deselected) {
        // Update the index before each selected/deselected range.
        for (const QItemSelectionRange &range : selected + deselected) {
          if (int row = range.top())
            update(this->model()->index(row - 1, 0));
        }

        // Assume this selection is deliberate
        mSelectionIsDefault = false;

        if (!mSuppressDiffDispatch)
          notifySelectionChanged();
      });

  setSelectionModel(selectionModel);

  restoreSelection();
}

void CommitList::contextMenuEvent(QContextMenuEvent *event) {
  QModelIndex index = indexAt(event->pos());
  if (!index.isValid())
    return;

  git::Reference ref = static_cast<CommitModel *>(mModel)->reference();
  CommitContextMenu menu(this, index, ref);
  menu.exec(event->globalPos());
}

void CommitList::mouseMoveEvent(QMouseEvent *event) {
  if (mStar.isValid() || mCancel.isValid())
    return;

  QListView::mouseMoveEvent(event);
}

void CommitList::mousePressEvent(QMouseEvent *event) {
  QPoint pos = event->pos();
  QModelIndex index = indexAt(pos);
  mStar = isStar(index, pos) ? index : QModelIndex();
  mCancel = isDecoration(index, pos) ? index : QModelIndex();

  if (mStar.isValid() || mCancel.isValid())
    return;

  DebugRefresh("time: " << QDateTime::currentDateTime());

  QListView::mousePressEvent(event);
}

void CommitList::mouseReleaseEvent(QMouseEvent *event) {
  QPoint pos = event->pos();
  QModelIndex index = indexAt(pos);
  if (mStar == index && isStar(index, pos)) {
    if (git::Commit commit = index.data(CommitRole).value<git::Commit>()) {
      commit.setStarred(!commit.isStarred());
      update(index); // FIXME: Add signal?
    }
  } else if (mCancel == index && isDecoration(index, pos)) {
    static_cast<CommitModel *>(model())->cancelStatus();
  }

  mStar = QModelIndex();
  mCancel = QModelIndex();

  QListView::mouseReleaseEvent(event);
}

void CommitList::leaveEvent(QEvent *event) {
  viewport()->update();
  QListView::leaveEvent(event);
}

void CommitList::paintEvent(QPaintEvent *event) {
  QListView::paintEvent(event);

  if (mLoading) {
    QPainter painter(viewport());
    QRect indicator(QPoint(0, 0), ProgressIndicator::size());
    indicator.moveCenter(viewport()->rect().center());
    ProgressIndicator::paint(&painter, indicator,
                             palette().color(QPalette::WindowText),
                             mLoadingFadein, mProgress);
  }
}

void CommitList::setLoading(bool loading) {
  if (loading == mLoading)
    return;

  mLoading = loading;
  if (loading) {
    mLoadingFadein = 0;
    mProgress = 0;
    mTimer.start(50);
  } else {
    mTimer.stop();
  }

  viewport()->update();
  emit loadingChanged(loading);
}

void CommitList::storeSelection() {
  // Don't pin the selection to a stale commit id across the reset: leave
  // mSelectedRange empty so restoreSelection() defers to the fallback
  // selection (selectFirstCommit(), triggered via statusFinished), which
  // picks up whatever the new default is
  mSelectedRange = mSelectionIsDefault ? QString() : selectedRange();
  DebugRefresh("Selected Range: " << mSelectedRange);
  Debug(mSelectedRange);
}

void CommitList::restoreSelection() {
  // Restore selection.
  DebugRefresh(mSelectedRange);
  // Restoring the same range after a reset doesn't need a fresh diff: the
  // commit(s) are immutable, so whatever was already displayed still applies.
  if (!mRestoreSelection ||
      (!mSelectedRange.isEmpty() && mSelectedRange != "status" &&
       !selectRange(mSelectedRange, QString(), false, false))) {
    DebugRefresh("Failed to restore");
    // Invalidate any in-flight async diff so a stale result for a
    // previously selected commit can't be delivered after this reset.
    ++mDiffRequest;
    emit diffSelected(git::Diff());
  }

  mSelectedRange = QString();

  if (selectedIndexes().isEmpty())
    selectFirstCommit();
}

void CommitList::updateModel() {
  if (!mFilter.isEmpty()) {
    setCommits(mIndex->commits(mFilter));
    return;
  }

  git::Reference ref = static_cast<CommitModel *>(mModel)->reference();
  if (ref.isValid() && ref.isStash()) {
    setCommits(ref.repo().stashes());
    return;
  }

  // Reset model.
  setModel(mModel);
}

QModelIndexList CommitList::sortedIndexes() const {
  QModelIndexList indexes = selectedIndexes();
  std::sort(indexes.begin(), indexes.end(),
            [](const QModelIndex &lhs, const QModelIndex &rhs) {
              return lhs.row() < rhs.row();
            });

  return indexes;
}

QModelIndex CommitList::findCommit(const git::Commit &commit) {
  // Get the 'uncommitted changes' index.
  QAbstractItemModel *model = this->model();
  if (!commit.isValid()) {
    QModelIndex index = model->index(0, 0);
    git::Commit tmp = index.data(CommitRole).value<git::Commit>();
    return !tmp.isValid() ? index : QModelIndex();
  }

  // Find the id.
  QDateTime date = commit.committer().date();
  for (int i = 0; i < model->rowCount(); ++i) {
    QModelIndex index = model->index(i, 0);
    if (git::Commit tmp = index.data(CommitRole).value<git::Commit>()) {
      if (tmp == commit)
        return index;

      // Cut off search if we find an older commit.
      if (tmp.committer().date() < date)
        return QModelIndex();
    }

    // Load more commits.
    if (i == model->rowCount() - 1 && model->canFetchMore(QModelIndex()))
      model->fetchMore(QModelIndex());
  }

  return QModelIndex();
}

void CommitList::selectIndexes(const QItemSelection &selection,
                               const QString &file, bool spontaneous,
                               bool dispatchDiff) {
  mFile = file;
  mSpontaneous = spontaneous;
  mSuppressDiffDispatch = !dispatchDiff;
  selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
  mSuppressDiffDispatch = false;
  mSpontaneous = true;
  mFile = QString();

  QModelIndexList indexes = selection.indexes();
  if (!indexes.isEmpty())
    scrollTo(indexes.first());
}

void CommitList::notifySelectionChanged() {
  // Multiple selection means that the selected parameter
  // could be empty when there are still indexes selected.
  QModelIndexList indexes = selectedIndexes();
  if (indexes.isEmpty())
    return;

  // Redraw all selected indexes. Separators may have changed.
  for (const QModelIndex &index : indexes)
    update(index);

  dispatchSelectedDiff(mFile, mSpontaneous);
}

void CommitList::dispatchSelectedDiff(const QString &file, bool spontaneous) {
  // Any in-flight request is now stale.
  int request = ++mDiffRequest;

  QModelIndexList indexes = sortedIndexes();
  if (indexes.isEmpty()) {
    emit diffSelected(git::Diff(), file, spontaneous);
    return;
  }

  // The uncommitted-changes row's diff is already computed asynchronously
  // elsewhere (CommitModel::status()); no need to compute it again.
  if (indexes.size() == 1) {
    git::Commit commit = indexes.first().data(CommitRole).value<git::Commit>();
    if (!commit.isValid()) {
      QVariant data = indexes.first().data(DiffRole);
      git::Diff diff = data.isValid() ? data.value<git::Diff>() : git::Diff();
      emit diffSelected(diff, file, spontaneous);
      return;
    }
  }

  git::Commit first = indexes.first().data(CommitRole).value<git::Commit>();
  if (!first.isValid()) {
    emit diffSelected(git::Diff(), file, spontaneous);
    return;
  }

  git::Commit last = indexes.last().data(CommitRole).value<git::Commit>();
  bool range = (indexes.size() > 1);
  bool ignoreWhitespace = Settings::instance()->isWhitespaceIgnored();

  // Let the diff/blame/file-list views clear themselves and show a loading
  // indicator while the (potentially slow) diff is computed.
  emit diffLoading();

  // Compute the diff and run rename detection off the GUI thread; this can
  // be slow for large commits/ranges. Discard the result if a newer
  // selection has superseded this request by the time it finishes.
  auto *watcher = new QFutureWatcher<git::Diff>(this);
  connect(watcher, &QFutureWatcher<git::Diff>::finished, watcher,
          [this, watcher, request, file, spontaneous] {
            git::Diff diff = watcher->result();
            watcher->deleteLater();
            // TODO: It would be great to have some cancel pathway instead of
            // doing this hack
            if (request == mDiffRequest)
              emit diffSelected(diff, file, spontaneous);
          });

  watcher->setFuture(QtConcurrent::run([first, last, range, ignoreWhitespace] {
    git::Diff diff = range ? first.diff(last, -1, ignoreWhitespace)
                           : first.diff(git::Commit(), -1, ignoreWhitespace);
    diff.findSimilar();
    return diff;
  }));
}

bool CommitList::isDecoration(const QModelIndex &index, const QPoint &pos) {
  if (!index.isValid())
    return false;

  CommitDelegate *delegate = static_cast<CommitDelegate *>(itemDelegate());
  QStyleOptionViewItem options;
  initViewItemOption(&options);
  options.rect = visualRect(index);
  return delegate->decorationRect(options, index).contains(pos);
}

bool CommitList::isStar(const QModelIndex &index, const QPoint &pos) {
  if (!index.isValid() || !index.data(CommitRole).isValid())
    return false;

  CommitDelegate *delegate = static_cast<CommitDelegate *>(itemDelegate());
  QStyleOptionViewItem options;
  initViewItemOption(&options);
  options.rect = visualRect(index);
  return delegate->starRect(options, index).contains(pos);
}
