//
//          Copyright (c) 2016, Scientific Toolworks, Inc.
//
// This software is licensed under the GNU General Public License v3.0 or
// (at your option) any later version. The LICENSE.md file describes the
// conditions under which this software may be distributed.
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Author: Shane Gramlich
//

#include "Test.h"
#include "log/LogEntry.h"
#include "qtsupport.h"
#include "ui/commits/CommitList.h"
#include "ui/detail/DetailView.h"
#include "ui/detail/DoubleTreeWidget.h"
#include "ui/diffView/DiffView.h"
#include "ui/filetree/TreeView.h"
#include "ui/repo/ConflictLabel.h"
#include "ui/repo/RepoView.h"
#include "ui/repo/StateBanner.h"
#include "ui/window/MainWindow.h"
#include "watcher/RepositoryWatcher.h"
#include <QApplication>
#include <QFile>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>

using namespace Test;
using namespace QTest;

class TestMerge : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void firstCommit();
  void secondCommit();
  void thirdCommit();
  void mergeConflict();
  void resolve();
  void revertConflict();
  void detachedMergeLabels();
  void stashConflictLabels();
  void stashPopKeepsStashOnConflict();
  void stashOverUncommittedChanges();
  void cleanupTestCase();

private:
  int inputDelay = 0;
  int closeDelay = 0;

  ScratchRepository mRepo;
  ScratchRepository mDetachedRepo;
  ScratchRepository mStashRepo;
  ScratchRepository mOverlapRepo;
  MainWindow *mWindow = nullptr;
  QString mMainBranch;
};

void TestMerge::initTestCase() {
  mMainBranch = mRepo->unbornHeadName();
  mWindow = new MainWindow(mRepo);
  mWindow->show();
  QVERIFY(qWaitForWindowExposed(mWindow));
}

void TestMerge::firstCommit() {
  // Add file and refresh.
  QFile file(mRepo->workdir().filePath("test"));
  QVERIFY(file.open(QFile::WriteOnly));
  QTextStream(&file) << "This will be a test." << Qt::endl;

  RepoView *view = mWindow->currentView();
  refresh(view);

  auto doubleTree = view->findChild<DoubleTreeWidget *>();
  QVERIFY(doubleTree);

  auto files = doubleTree->findChild<TreeView *>("Unstaged");
  QVERIFY(files);

  QAbstractItemModel *model = files->model();
  QCOMPARE(model->rowCount(), 1);

  // Click on the check box.
  QModelIndex index = model->index(0, 0);
  mouseClick(files->viewport(), Qt::LeftButton, Qt::KeyboardModifiers(),
             files->checkRect(index).center());

  // Commit and refresh.
  QTextEdit *editor = view->findChild<QTextEdit *>("MessageEditor");
  QVERIFY(editor);

  editor->setText("base commit");
  view->commit();
  refresh(view, false);
}

void TestMerge::secondCommit() {
  RepoView *view = mWindow->currentView();
  git::Branch branch = mRepo->createBranch("branch2", mRepo->head().target());
  QVERIFY(branch.isValid());

  view->checkout(branch);
  QCOMPARE(mRepo->head().name(), QString("branch2"));

  QFile file(mRepo->workdir().filePath("test"));
  QVERIFY(file.open(QFile::WriteOnly));
  QTextStream(&file) << "This is a conflict." << Qt::endl;

  refresh(view);

  auto doubleTree = view->findChild<DoubleTreeWidget *>();
  QVERIFY(doubleTree);

  auto files = doubleTree->findChild<TreeView *>("Unstaged");
  QVERIFY(files);

  QAbstractItemModel *model = files->model();
  QCOMPARE(model->rowCount(), 1);

  // Click on the check box.
  QModelIndex index = model->index(0, 0);
  mouseClick(files->viewport(), Qt::LeftButton, Qt::KeyboardModifiers(),
             files->checkRect(index).center());

  // Commit and refresh.
  QTextEdit *editor = view->findChild<QTextEdit *>("MessageEditor");
  QVERIFY(editor);

  editor->setText("conflicting commit b");
  view->commit();
  refresh(view, false);
}

void TestMerge::thirdCommit() {
  RepoView *view = mWindow->currentView();
  git::Reference ref =
      mRepo->lookupRef(QString("refs/heads/%1").arg(mMainBranch));
  QVERIFY(ref);

  view->checkout(ref);
  QCOMPARE(mRepo->head().name(), mMainBranch);

  QFile file(mRepo->workdir().filePath("test"));
  QVERIFY(file.open(QFile::WriteOnly));
  QTextStream(&file) << "This is a test." << Qt::endl;

  refresh(view);

  auto doubleTree = view->findChild<DoubleTreeWidget *>();
  QVERIFY(doubleTree);

  auto files = doubleTree->findChild<TreeView *>("Unstaged");
  QVERIFY(files);

  QAbstractItemModel *model = files->model();
  QCOMPARE(model->rowCount(), 1);

  // Click on the check box.
  QModelIndex index = model->index(0, 0);
  mouseClick(files->viewport(), Qt::LeftButton, Qt::KeyboardModifiers(),
             files->checkRect(index).center());

  // Commit and refresh.
  QTextEdit *editor = view->findChild<QTextEdit *>("MessageEditor");
  QVERIFY(editor);

  editor->setText("conflicting commit a");
  view->commit();
  refresh(view, false);
}

void TestMerge::mergeConflict() {
  RepoView *view = mWindow->currentView();
  git::Reference master =
      mRepo->lookupRef(QString("refs/heads/%1").arg(mMainBranch));
  QVERIFY(master);

  git::Reference branch2 = mRepo->lookupRef("refs/heads/branch2");
  QVERIFY(branch2);

  QCOMPARE(mRepo->head().name(), mMainBranch);

  view->merge(RepoView::Merge, branch2);

  // Diff is in a conflicted state
  git::Diff diff = mRepo->diffIndexToWorkdir();
  QVERIFY(diff.isConflicted());

  // Wait for the commit editor to report the conflict.
  DetailView *detailView = view->findChild<DetailView *>();
  QVERIFY(detailView);
  auto hasConflictStatus = [detailView] {
    for (QLabel *label : detailView->findChildren<QLabel *>()) {
      if (label->text().contains("unresolved conflict"))
        return true;
    }
    return false;
  };
  QTRY_VERIFY_WITH_TIMEOUT(hasConflictStatus(), 10000);

  // Commit is not available while conflicts remain.
  QPushButton *commit = nullptr;
  for (QPushButton *button : detailView->findChildren<QPushButton *>()) {
    if (button->text() == "Commit Merge")
      commit = button;
  }
  QVERIFY(commit);

  QTextEdit *editor = view->findChild<QTextEdit *>("MessageEditor");
  QVERIFY(editor);

  // Git's commented conflict list isn't offered as part of the message.
  QTRY_COMPARE(editor->toPlainText(), QString("Merge branch 'branch2'"));
  editor->clear();
  editor->setText("merge commit");
  QVERIFY(!commit->isEnabled());
  QVERIFY(commit->toolTip().contains("Resolve the remaining conflicts"));

  // The banner says so too, wherever the user is looking.
  StateBanner *banner = view->findChild<StateBanner *>();
  QVERIFY(banner);
  QTRY_VERIFY(banner->isVisible());
  // Tests run untranslated, so the count shows its plural source text.
  QCOMPARE(
      banner->message(),
      QString("Merging branch2 into %1. 1 file(s) have conflicts. Keep one "
              "version or edit it, then stage it to mark it resolved. "
              "Finally, click Commit Merge to finish the merge.")
          .arg(mMainBranch));

  // The uncommitted changes row is named after the merge.
  QAbstractItemModel *commits = view->findChild<CommitList *>()->model();
  QTRY_COMPARE(commits->index(0, 0).data().toString(),
               QString("Merge in progress"));

  // Its main action leads to the conflict, even from another commit.
  auto doubleTree = view->findChild<DoubleTreeWidget *>();
  QVERIFY(doubleTree);
  auto files = doubleTree->findChild<TreeView *>("Unstaged");
  QVERIFY(files);
  CommitList *commitList = view->findChild<CommitList *>();
  QVERIFY(commitList);
  commitList->setCurrentIndex(commitList->model()->index(1, 0));
  QTRY_VERIFY(!editor->isVisible());

  QPushButton *show = nullptr;
  auto findShow = [&] {
    for (QPushButton *button : banner->findChildren<QPushButton *>()) {
      if (button->isVisibleTo(banner) && button->text() == tr("Show Conflicts"))
        show = button;
    }
    return show != nullptr;
  };
  QTRY_VERIFY(findShow());
  show->click();
  QTRY_VERIFY(editor->isVisible());
  QTRY_COMPARE(files->currentIndex().data(Qt::DisplayRole).toString(),
               QString("test"));

  // It still works when the uncommitted changes are already shown.
  QSignalSpy dispatched(commitList, &CommitList::diffSelected);
  show->click();
  QCOMPARE(dispatched.count(), 1);
  QCOMPARE(dispatched.first().at(1).toString(), QString("test"));

  // Staging a file that still has conflict markers asks first. Cancel before
  // checking, as a failed check would leave the modal dialog open.
  QString prompt;
  QTimer::singleShot(0, [&prompt] {
    auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
    if (!box)
      return;
    prompt = box->text();
    box->button(QMessageBox::Cancel)->click();
  });
  mRepo->index().setStaged({"test"}, true);
  QCOMPARE(prompt, QString("'test' still contains conflict markers (<<<<<<<, "
                           "=======, >>>>>>>)."));
  QVERIFY(mRepo->index().hasConflicts());

  // Aborting asks first, and cancelling leaves the merge alone.
  view->promptToAbort();
  QMessageBox *confirm = nullptr;
  QTRY_VERIFY((confirm = view->findChild<QMessageBox *>()));
  QString question = confirm->text();
  confirm->button(QMessageBox::Cancel)->click();
  QCOMPARE(question, QString("Are you sure you want to abort the merge?"));
  QCOMPARE(mRepo->state(), GIT_REPOSITORY_STATE_MERGE);
  QCOMPARE(mRepo->operation(), git::Operation::Merge);
}

void TestMerge::resolve() {
  RepoView *view = mWindow->currentView();
  DiffView *diffView = view->findChild<DiffView *>();

  auto doubleTree = view->findChild<DoubleTreeWidget *>();
  QVERIFY(doubleTree);

  auto files = doubleTree->findChild<TreeView *>("Unstaged");
  QVERIFY(files);

  // Wait for refresh
  QAbstractItemModel *model = files->model();
  QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() >= 1, 10000);

  files->selectionModel()->select(files->model()->index(0, 0),
                                  QItemSelectionModel::Select);

  // The diff loads asynchronously, so the buttons don't exist right away.
  QToolButton *theirs = nullptr;
  QTRY_VERIFY_WITH_TIMEOUT(
      (theirs = diffView->findChild<QToolButton *>("ConflictTheirs")), 10000);

  QToolButton *ours =
      diffView->widget()->findChild<QToolButton *>("ConflictOurs");
  QVERIFY(ours);

  // Named by branch, not by the ambiguous "ours"/"theirs" pronouns. "Ours"
  // isn't inverted during a plain merge, unlike during a rebase: it's still
  // the branch merged into (master), not the branch merged in.
  QCOMPARE(ours->text(), QString("Keep %1").arg(mMainBranch));
  QCOMPARE(theirs->text(), QString("Take branch2"));

  // How to resolve it is explained next to the file, not only in a log
  // panel that's about to slide away.
  QWidget *hint = diffView->widget()->findChild<QWidget *>("ConflictHint");
  QVERIFY(hint);
  QVERIFY(hint->isVisible());
  QStringList hintTexts;
  for (QLabel *label : hint->findChildren<QLabel *>())
    hintTexts.append(label->text());
  QVERIFY(hintTexts.contains(
      QString("Click Keep %1 or Take branch2, then Save. Or edit the file "
              "yourself, or use External Merge. When it's done, stage the "
              "file to mark it resolved.")
          .arg(mMainBranch)));

  // External Merge is a single file-wide button, not one per conflict.
  QList<QToolButton *> externalMerge =
      diffView->widget()->findChildren<QToolButton *>("ConflictExternalMerge");
  QCOMPARE(externalMerge.count(), 1);
  QVERIFY(externalMerge.first()->isVisible());
  QVERIFY(externalMerge.first()->isEnabled());

  mouseClick(theirs, Qt::LeftButton, Qt::KeyboardModifiers(), QPoint(),
             inputDelay);

  QToolButton *undo =
      diffView->widget()->findChild<QToolButton *>("ConflictUndo");
  QVERIFY(undo);
  mouseClick(undo, Qt::LeftButton, Qt::KeyboardModifiers(), QPoint(),
             inputDelay);

  mouseClick(ours, Qt::LeftButton, Qt::KeyboardModifiers(), QPoint(),
             inputDelay);

  QToolButton *save =
      diffView->widget()->findChild<QToolButton *>("ConflictSave");
  QVERIFY(save);
  mouseClick(save, Qt::LeftButton, Qt::KeyboardModifiers(), QPoint(),
             inputDelay);

  // Once saved, only staging is left, and the guidance says just that.
  auto hintText = [diffView] {
    QStringList texts;
    if (QWidget *hint =
            diffView->widget()->findChild<QWidget *>("ConflictHint")) {
      for (QLabel *label : hint->findChildren<QLabel *>()) {
        if (!label->text().isEmpty())
          texts.append(label->text());
      }
    }
    return texts.join(" ");
  };
  QTRY_COMPARE_WITH_TIMEOUT(
      hintText(),
      QString("No conflicts left in this file. Stage it to mark it resolved."),
      10000);
  for (const char *name :
       {"ConflictFileOurs", "ConflictFileTheirs", "ConflictExternalMerge"}) {
    QToolButton *button = diffView->widget()->findChild<QToolButton *>(name);
    QVERIFY(button);
    QVERIFY(button->isHidden());
  }

  DetailView *detailView = view->findChild<DetailView *>();
  QPushButton *stageAll = nullptr;
  QTRY_VERIFY_WITH_TIMEOUT(
      (stageAll = detailView->findChild<QPushButton *>("StageAll")), 10000);
  // Stage after the watcher's refresh for Save, as a user usually would.
  RepositoryWatcher *watcher = nullptr;
  for (QObject *child : view->children()) {
    if (auto *candidate = dynamic_cast<RepositoryWatcher *>(child))
      watcher = candidate;
  }
  QVERIFY(watcher);
  watcher->cancelPendingNotification();
  mouseClick(stageAll, Qt::LeftButton, Qt::KeyboardModifiers(), QPoint(),
             inputDelay);

  // Staging clears the conflict from the view without a manual refresh.
  QTRY_VERIFY_WITH_TIMEOUT(hintText().isEmpty(), 10000);

  // Keeping master leaves nothing that differs from it, yet the merge still
  // has to be committed.
  QAbstractItemModel *commits = view->findChild<CommitList *>()->model();
  QTRY_COMPARE(commits->index(0, 0).data().toString(),
               QString("Merge ready to commit"));
  QTRY_VERIFY(diffView->widget()->findChild<QLabel *>("MergeWithoutChanges"));

  QTextEdit *editor = view->findChild<QTextEdit *>("MessageEditor");
  QVERIFY(editor);

  // With the conflicts gone, the banner points to the commit message.
  StateBanner *banner = view->findChild<StateBanner *>();
  QTRY_COMPARE(banner->message(),
               QString("Merging branch2 into %1. No conflicts left and no "
                       "file changes. Click Commit Merge to record the merge.")
                   .arg(mMainBranch));

  // Buttons added to a visible banner are shown on the next event loop turn.
  QPushButton *show = nullptr;
  auto findShow = [&] {
    for (QPushButton *button : banner->findChildren<QPushButton *>()) {
      if (button->isVisibleTo(banner) && button->text() == "Show Changes")
        show = button;
    }
    return show != nullptr;
  };
  QTRY_VERIFY(findShow());

  // The offscreen window is never active, so check the window's focus widget.
  files->setFocus();
  QCOMPARE(editor->window()->focusWidget(), files);
  show->click();
  QTRY_COMPARE(editor->window()->focusWidget(), editor);

  // Commit and refresh.
  editor->setText("conflicts resolved");
  QTRY_VERIFY(view->isCommitEnabled());
  view->commit();
  refresh(view, false);

  // Diff is not in a conflicted state
  git::Diff diff = mRepo->diffIndexToWorkdir();
  QVERIFY(!diff.isConflicted());

  QTRY_VERIFY(!view->findChild<StateBanner *>()->isVisible());
  QCOMPARE(mRepo->head().target().parents().count(), 2);

  // The hint goes away once the conflict does.
  QVERIFY(!diffView->widget()->findChild<QWidget *>("ConflictHint"));
}

void TestMerge::revertConflict() {
  RepoView *view = mWindow->currentView();

  // Change the line again, so reverting "conflicting commit a" conflicts.
  QFile file(mRepo->workdir().filePath("test"));
  QVERIFY(file.open(QFile::WriteOnly));
  QTextStream(&file) << "This is something else." << Qt::endl;
  file.close();
  mRepo->index().setStaged({"test"}, true);
  QVERIFY(mRepo->commit("something else", git::AnnotatedCommit()).isValid());

  git::Commit merge = mRepo->head().target().parents().first();
  git::Commit commitA = merge.parents().first();
  QCOMPARE(commitA.summary(), QString("conflicting commit a"));

  view->revert(commitA);
  QVERIFY(mRepo->index().hasConflicts());
  refresh(view);

  // The incoming side of a revert lacks the commit's change, so say "Undo".
  QString undo = QString("Undo commit %1").arg(commitA.shortId());
  auto [oursLabel, theirsLabel] = conflict::labels(view->repo());
  QCOMPARE(theirsLabel, undo);
  DiffView *diffView = view->findChild<DiffView *>();
  QToolButton *theirs = nullptr;
  QTRY_VERIFY_WITH_TIMEOUT(
      (theirs = diffView->findChild<QToolButton *>("ConflictTheirs")), 10000);
  QCOMPARE(theirs->text(), undo);
}

namespace {

void writeFile(git::Repository &repo, const char *text) {
  QFile file(repo.workdir().filePath("f"));
  QVERIFY(file.open(QFile::WriteOnly));
  file.write(text);
}

void commitFile(git::Repository &repo, const char *message) {
  repo.index().setStaged({"f"}, true);
  QVERIFY(repo.commit(message, git::AnnotatedCommit()).isValid());
}

RepoView *openWindow(git::Repository &repo) {
  MainWindow *window = new MainWindow(repo);
  window->show();
  if (!qWaitForWindowExposed(window))
    return nullptr;
  RepoView *view = window->currentView();
  refresh(view, false);
  return view;
}

} // namespace

void TestMerge::detachedMergeLabels() {
  git::Repository repo = mDetachedRepo;
  QString main = repo.unbornHeadName();
  writeFile(repo, "base\n");
  commitFile(repo, "base");
  RepoView *view = openWindow(repo);
  QVERIFY(view);

  view->checkout(repo.createBranch("other", repo.head().target()));
  refresh(view, false);
  writeFile(repo, "theirs\n");
  commitFile(repo, "theirs");
  view->checkout(repo.lookupRef(QString("refs/heads/%1").arg(main)));
  refresh(view, false);
  writeFile(repo, "ours\n");
  commitFile(repo, "ours");

  // Without a branch there's no name for this side, so describe it instead.
  QVERIFY(repo.setHeadDetached(repo.head().target()));
  view->merge(RepoView::Merge, repo.lookupRef("refs/heads/other"));
  QVERIFY(repo.index().hasConflicts());
  auto [oursLabel, theirsLabel] = conflict::labels(view->repo());
  QCOMPARE(oursLabel, QString("Keep current version"));
  QCOMPARE(theirsLabel, QString("Take other"));
  view->window()->close();
}

void TestMerge::stashConflictLabels() {
  git::Repository repo = mStashRepo;
  writeFile(repo, "base\n");
  commitFile(repo, "base");
  RepoView *view = openWindow(repo);
  QVERIFY(view);

  writeFile(repo, "stashed\n");
  QVERIFY(repo.stash("stashed").isValid());
  refresh(view, false);
  writeFile(repo, "other\n");
  commitFile(repo, "other");

  // Applying the stash conflicts without any operation in progress.
  view->applyStash(0);
  QVERIFY(repo.index().hasConflicts());
  QCOMPARE(repo.state(), GIT_REPOSITORY_STATE_NONE);
  QCOMPARE(repo.operation(), git::Operation::None);
  auto [oursLabel, theirsLabel] = conflict::labels(view->repo());
  QCOMPARE(oursLabel, QString("Keep %1").arg(repo.head().name()));
  QCOMPARE(theirsLabel, QString("Take stashed version"));

  // The banner says what happened, and only offers to show the conflicts.
  StateBanner *banner = view->findChild<StateBanner *>();
  QTRY_COMPARE(banner->message(),
               QString("Applying the stash caused conflicts. 1 file(s) have "
                       "conflicts. Keep one version or edit it, then stage it "
                       "to mark it resolved. Your stash is kept, so nothing is "
                       "lost."));
  auto buttons = [banner] {
    QStringList texts;
    for (QPushButton *button : banner->findChildren<QPushButton *>()) {
      if (button->isVisibleTo(banner))
        texts.append(button->text());
    }
    return texts;
  };
  QTRY_COMPARE(buttons(), QStringList({"Show Conflicts"}));

  // Resolving the conflict ends it.
  writeFile(repo, "resolved\n");
  repo.index().setStaged({"f"}, true);
  QTRY_VERIFY(!banner->isVisible());
  view->window()->close();
}

void TestMerge::stashPopKeepsStashOnConflict() {
  // Like git, a conflicting pop keeps the stash, while a clean one drops it.
  ScratchRepository conflicting;
  git::Repository repo = conflicting;
  writeFile(repo, "base\n");
  commitFile(repo, "base");
  writeFile(repo, "stashed\n");
  QVERIFY(repo.stash("stashed").isValid());
  writeFile(repo, "other\n");
  commitFile(repo, "other");
  QVERIFY(repo.popStash());
  QVERIFY(repo.index().hasConflicts());
  QCOMPARE(repo.stashes().size(), 1);

  ScratchRepository clean;
  repo = clean;
  writeFile(repo, "base\n");
  commitFile(repo, "base");
  writeFile(repo, "stashed\n");
  QVERIFY(repo.stash("stashed").isValid());
  QVERIFY(repo.popStash());
  QVERIFY(!repo.index().hasConflicts());
  QCOMPARE(repo.stashes().size(), 0);
}

void TestMerge::stashOverUncommittedChanges() {
  git::Repository repo = mOverlapRepo;
  writeFile(repo, "base\n");
  commitFile(repo, "base");
  RepoView *view = openWindow(repo);
  QVERIFY(view);

  writeFile(repo, "stashed\n");
  QVERIFY(repo.stash("stashed").isValid());
  refresh(view, false);
  writeFile(repo, "local\n");

  // The last error entry in the log, found through the log's root.
  auto lastError = [view] {
    LogEntry *root = view->addLogEntry("", "")->parentEntry();
    for (int i = root->entries().size() - 1; i >= 0; --i) {
      for (LogEntry *child : root->entries().at(i)->entries()) {
        if (child->kind() == LogEntry::Error)
          return child->text();
      }
    }
    return QString();
  };

  // Neither applying nor popping overwrites the uncommitted change, and the
  // log says why.
  QString expected("Can't apply the stash, because it would overwrite your "
                   "uncommitted changes to f. Commit or stash those changes "
                   "first, then try again.");
  view->popStash(0);
  QCOMPARE(lastError(), expected);
  view->applyStash(0);
  QCOMPARE(lastError(), expected);

  QCOMPARE(repo.stashes().size(), 1);
  QFile file(repo.workdir().filePath("f"));
  QVERIFY(file.open(QFile::ReadOnly));
  QCOMPARE(file.readAll(), QByteArray("local\n"));
  view->window()->close();
}

void TestMerge::cleanupTestCase() {
  qWait(closeDelay);
  mWindow->close();
}

TEST_MAIN(TestMerge)

#include "merge.moc"
