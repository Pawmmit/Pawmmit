//
//          Copyright (c) 2022, Pawmmit Community
//
// This software is licensed under the GNU General Public License v3.0 or
// (at your option) any later version. The LICENSE.md file describes the
// conditions under which this software may be distributed.
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Author: Martin Marmsoler
//

#include "Test.h"
#include "ui/detail/DetailView.h"
#include "ui/detail/DoubleTreeWidget.h"
#include "ui/diffView/DiffView.h"
#include "ui/repo/RepoView.h"

#include "ui/detail/DetailView.h"
#include "ui/diffView/FileWidget.h"
#include "ui/diffView/HunkWidget.h"
#include "ui/repo/StateBanner.h"
#include "ui/window/MainWindow.h"
#include "ui/window/MenuBar.h"

#include "git/Commit.h"
#include "git/Diff.h"
#include "git/Patch.h"
#include "git/Reference.h"
#include "git/Tree.h"

#include "log/LogEntry.h"

#include <QPushButton>
#include <QStackedWidget>
#include <QTextEdit>
#include <QToolButton>

#define INIT_REPO(repoPath)                                                    \
  QString path = Test::extractRepository(repoPath);                            \
  QVERIFY(!path.isEmpty());                                                    \
  mRepo = git::Repository::open(path);                                         \
  QVERIFY(mRepo.isValid());                                                    \
  Test::initRepo(mRepo);                                                       \
  MainWindow window(mRepo);                                                    \
  window.show();                                                               \
  QVERIFY(QTest::qWaitForWindowExposed(&window));                              \
                                                                               \
  git::Reference head = mRepo.head();                                          \
  git::Commit commit = head.target();                                          \
  git::Diff stagedDiff = mRepo.diffTreeToIndex(commit.tree()); /* correct */   \
                                                                               \
  RepoView *repoView = window.currentView();                                   \
  auto diff = mRepo.status(mRepo.index(), nullptr, false);

using namespace git;

class TestRebase : public QObject {
  Q_OBJECT

private slots:
  void withoutConflicts();
  void conflictingRebase();
  void conflictingRebaseCustomMessage();
  void continueExternalStartedRebase(); // must have conflicts otherwise it is
                                        // not possible to continue
  void startRebaseContinueExternally();
  void startRebaseContinueExternallyContinueGUI(); // start rebase, commit
                                                   // externally and finish in
                                                   // the GUI
  void abortMR();
  void commitDuringRebase();

private:
  void startExternalRebase(git::Repository &external);
  void startGuiRebase(RepoView *repoView);
  void resolveConflict(git::Repository &repo);

  git::Repository mRepo;
};

// ###################################################################################################
// ###################################################################################################
// ###################################################################################################
//  Tests starting
//  ######################################################################################
// ###################################################################################################
// ###################################################################################################
// ###################################################################################################

void TestRebase::withoutConflicts() {
  INIT_REPO("rebaseConflicts.zip");

  int rebaseFinished = 0;
  int rebaseAboutToRebase = 0;
  int rebaseCommitSuccess = 0;
  int rebaseConflict = 0;

  connect(mRepo.notifier(), &git::RepositoryNotifier::rebaseInitError,
          [=]() { QVERIFY(false); }); // Should not be called
  connect(mRepo.notifier(), &git::RepositoryNotifier::rebaseAboutToRebase,
          [=, &rebaseAboutToRebase](const Rebase rebase, const Commit before,
                                    int count) {
            QVERIFY(rebase.isValid());
            QCOMPARE(count, 1);
            QCOMPARE(before.message(), "File2.txt added");
            rebaseAboutToRebase++;
          });
  connect(mRepo.notifier(), &git::RepositoryNotifier::rebaseCommitInvalid,
          [=]() { QVERIFY(false); }); // Should not be called
  connect(mRepo.notifier(), &git::RepositoryNotifier::rebaseFinished,
          [=, &rebaseFinished]() { rebaseFinished++; });
  connect(mRepo.notifier(), &git::RepositoryNotifier::rebaseCommitSuccess,
          [=, &rebaseCommitSuccess](const Rebase rebase, const Commit before,
                                    const Commit after, int counter) {
            QVERIFY(rebase.isValid());
            rebaseCommitSuccess++;
          });
  connect(mRepo.notifier(), &git::RepositoryNotifier::rebaseConflict,
          [=, &rebaseConflict]() { rebaseConflict++; });

  const QString rebaseBranchName = "refs/heads/noConflict";

  git::Reference branch = mRepo.lookupRef(rebaseBranchName);
  QVERIFY(branch.isValid());
  auto c = branch.annotatedCommit().commit();

  LogEntry *entry = repoView->addLogEntry("Rebase", "Rebase", nullptr);

  // Checkout correct branch
  QCOMPARE(mRepo.checkout(c), true);

  // Rebase on main
  git::Reference mainBranch = mRepo.lookupRef(QString("refs/heads/main"));
  QVERIFY(mainBranch.isValid());
  auto ac = mainBranch.annotatedCommit();
  repoView->rebase(ac, entry);

  // Check that branch is based on "main" now
  branch = mRepo.lookupRef(rebaseBranchName);
  QVERIFY(branch.isValid());
  QList<Commit> parents = branch.annotatedCommit().commit().parents();
  QCOMPARE(parents.count(), 1);
  QCOMPARE(parents.at(0).id(), ac.commit().id());

  // Check that rebase was really finished
  QCOMPARE(mRepo.rebaseOngoing(), false);

  // Check call counters
  QCOMPARE(rebaseFinished, 1);
  QCOMPARE(rebaseAboutToRebase, 1);
  QCOMPARE(rebaseCommitSuccess, 1);
  QCOMPARE(rebaseConflict, 0);

  auto *detailview = repoView->findChild<DetailView *>();
  QVERIFY(detailview);
  auto *rebaseBanner = repoView->findChild<StateBanner *>();
  QVERIFY(rebaseBanner);
  auto *continueRebaseButton =
      detailview->findChild<QPushButton *>("ContinueRebase");
  QVERIFY(continueRebaseButton);
  QCOMPARE(continueRebaseButton->isVisible(), false);
  QCOMPARE(rebaseBanner->isVisible(), false);
}

void TestRebase::conflictingRebase() {
  INIT_REPO("rebaseConflicts.zip");

  auto *detailview = repoView->findChild<DetailView *>();
  QVERIFY(detailview);
  auto *rebaseBanner = repoView->findChild<StateBanner *>();
  QVERIFY(rebaseBanner);
  auto *continueRebaseButton =
      detailview->findChild<QPushButton *>("ContinueRebase");
  QVERIFY(continueRebaseButton);
  QCOMPARE(continueRebaseButton->isVisible(), false);
  QCOMPARE(rebaseBanner->isVisible(), false);

  int rebaseFinished = 0;
  int rebaseAboutToRebase = 0;
  int rebaseCommitSuccess = 0;
  int rebaseConflict = 0;
  int refreshTriggered = 0;

  connect(mRepo.notifier(), &git::RepositoryNotifier::rebaseInitError,
          [=]() { QVERIFY(false); }); // Should not be called
  connect(mRepo.notifier(), &git::RepositoryNotifier::rebaseAboutToRebase,
          [=, &rebaseAboutToRebase](const Rebase rebase, const Commit before,
                                    int count) {
            QVERIFY(rebase.isValid());
            QCOMPARE(count, 1);
            QCOMPARE(before.message(), "File.txt changed by second branch\n");
            rebaseAboutToRebase++;
          });
  // TODO: = needed?
  connect(mRepo.notifier(), &git::RepositoryNotifier::rebaseCommitInvalid,
          [=]() { QVERIFY(false); }); // Should not be called
  connect(mRepo.notifier(), &git::RepositoryNotifier::rebaseFinished,
          [=, &rebaseFinished]() { rebaseFinished++; });
  connect(mRepo.notifier(), &git::RepositoryNotifier::rebaseCommitSuccess,
          [=, &rebaseCommitSuccess](const Rebase rebase, const Commit before,
                                    const Commit after, int counter) {
            QVERIFY(rebase.isValid());
            rebaseCommitSuccess++;
          });
  connect(mRepo.notifier(), &git::RepositoryNotifier::rebaseConflict,
          [=, &rebaseConflict, &rebaseCommitSuccess]() {
            QCOMPARE(rebaseCommitSuccess, 0); // was not called yet
            rebaseConflict++;
          });

  connect(mRepo.notifier(), &RepositoryNotifier::referenceUpdated,
          [this, &refreshTriggered](const Reference &ref) {
            // TODO: enable
            // QCOMPARE(ref, mRepo.head());
            refreshTriggered++;
          });

  const QString rebaseBranchName = "refs/heads/singleCommitConflict";

  git::Reference branch = mRepo.lookupRef(rebaseBranchName);
  QVERIFY(branch.isValid());
  auto c = branch.annotatedCommit().commit();

  // Checkout correct branch
  repoView->checkout(branch);

  // Rebase on main
  git::Reference mainBranch = mRepo.lookupRef(QString("refs/heads/main"));
  QVERIFY(mainBranch.isValid());
  auto ac = mainBranch.annotatedCommit();
  refreshTriggered = 0;
  LogEntry *entry = repoView->addLogEntry("Rebase", "Rebase", nullptr);
  repoView->rebase(ac, entry);
  QCOMPARE(refreshTriggered, 1); // Check that refresh was triggered

  QCOMPARE(mRepo.rebaseOngoing(), true);
  QCOMPARE(rebaseFinished, 0);
  QCOMPARE(rebaseConflict, 1);

  // Check that buttons are visible
  QTRY_COMPARE(continueRebaseButton->isVisible(), true);
  QTRY_COMPARE(rebaseBanner->isVisible(), true);

  // The Branch menu can abort the rebase too.
  MenuBar *menuBar = MenuBar::instance(&window);
  menuBar->update();
  QAction *abortAction = nullptr;
  for (QAction *action : menuBar->findChildren<QAction *>()) {
    if (action->text() == "Abort Rebase")
      abortAction = action;
  }
  QVERIFY(abortAction);
  QVERIFY(abortAction->isEnabled());

  // The banner says what's happening in plain words, and leads to the
  // conflict rather than offering to continue past it.
  QTRY_VERIFY(rebaseBanner->isVisible());
  // Tests run untranslated, so the count shows its plural source text.
  QCOMPARE(
      rebaseBanner->message(),
      QString("Rebasing singleCommitConflict onto main, commit 1 of 1. "
              "1 file(s) have conflicts. Keep one version or edit it, then "
              "stage it to mark it resolved. Finally, click Continue "
              "Rebase next to the commit message."));
  QStringList bannerButtons;
  for (QPushButton *button : rebaseBanner->findChildren<QPushButton *>()) {
    if (button->isVisibleTo(rebaseBanner))
      bannerButtons.append(button->text());
  }
  QCOMPARE(bannerButtons, QStringList({"Show Conflicts", "Abort Rebase"}));

  // The one next to the commit message agrees with the banner.
  QTRY_VERIFY(!continueRebaseButton->isEnabled());

  // Resolve conflicts
  diff = mRepo.status(mRepo.index(), nullptr, false);
  QCOMPARE(diff.count(), 1);
  QCOMPARE(diff.patch(0).isConflicted(), true);
  QFile f(mRepo.workdir().filePath(diff.patch(0).name()));
  QCOMPARE(f.open(QIODevice::WriteOnly), true);
  QVERIFY(f.write("Test123") !=
          -1); // just write something to resolve the conflict
  f.close();

  refreshTriggered = 0;
  repoView->continueRebase();  // should fail
  QCOMPARE(rebaseConflict, 2); // User tries to continue without staging
  QCOMPARE(refreshTriggered, 1);

  // Staging the file
  QTRY_COMPARE(repoView->findChildren<FileWidget *>().length(), 1);
  auto filewidgets = repoView->findChildren<FileWidget *>();

  // During a rebase, git's own "ours"/"theirs" are swapped relative to a
  // merge: "ours" is main, the branch being rebased onto, not the branch
  // ("singleCommitConflict") the user actually checked out and is rebasing.
  // The button labels must say so plainly rather than repeat that swap.
  QToolButton *ours =
      filewidgets.at(0)->findChild<QToolButton *>("ConflictFileOurs");
  QToolButton *theirs =
      filewidgets.at(0)->findChild<QToolButton *>("ConflictFileTheirs");
  QVERIFY(ours);
  QVERIFY(theirs);
  QCOMPARE(ours->text(), QString("Keep main"));
  QCOMPARE(theirs->text(), QString("Take singleCommitConflict"));

  filewidgets.at(0)->stageStateChanged(filewidgets.at(0)->modelIndex(),
                                       git::Index::StagedState::Staged);
  QTRY_VERIFY(continueRebaseButton->isEnabled());

  // The banner then only navigates, so there's a single Continue button.
  QTRY_VERIFY(rebaseBanner->message().contains("No conflicts left."));
  auto visibleButtons = [&] {
    QStringList texts;
    for (QPushButton *button : rebaseBanner->findChildren<QPushButton *>()) {
      if (button->isVisibleTo(rebaseBanner))
        texts.append(button->text());
    }
    return texts;
  };
  QTRY_COMPARE(visibleButtons(), QStringList({"Show Changes", "Abort Rebase"}));

  refreshTriggered = 0;
  rebaseConflict = 0;
  repoView->continueRebase();
  QCOMPARE(refreshTriggered, 1);

  // Check that branch is based on "main" now
  branch = mRepo.lookupRef(rebaseBranchName);
  QVERIFY(branch.isValid());
  QList<Commit> parents = branch.annotatedCommit().commit().parents();
  QCOMPARE(parents.count(), 1);
  QCOMPARE(parents.at(0).id(), ac.commit().id());
  QCOMPARE(branch.annotatedCommit().commit().message(),
           "File.txt changed by second branch"); // original message is shown

  // Check that rebase was really finished
  QCOMPARE(mRepo.rebaseOngoing(), false);

  // Check that buttons are visible
  QTRY_COMPARE(continueRebaseButton->isVisible(), false);
  QTRY_COMPARE(rebaseBanner->isVisible(), false);

  // Check call counters
  QCOMPARE(rebaseFinished, 1);
  QCOMPARE(rebaseAboutToRebase, 1);
  QCOMPARE(rebaseCommitSuccess, 1);
  QCOMPARE(rebaseConflict, 0);
}

void TestRebase::conflictingRebaseCustomMessage() {
  INIT_REPO("rebaseConflicts.zip");

  auto *detailview = repoView->findChild<DetailView *>();
  QVERIFY(detailview);
  auto *rebaseBanner = repoView->findChild<StateBanner *>();
  QVERIFY(rebaseBanner);
  auto *continueRebaseButton =
      detailview->findChild<QPushButton *>("ContinueRebase");
  QVERIFY(continueRebaseButton);
  QCOMPARE(continueRebaseButton->isVisible(), false);
  QCOMPARE(rebaseBanner->isVisible(), false);

  const QString rebaseBranchName = "refs/heads/singleCommitConflict";

  git::Reference branch = mRepo.lookupRef(rebaseBranchName);
  QVERIFY(branch.isValid());
  auto c = branch.annotatedCommit().commit();

  // Checkout correct branch
  repoView->checkout(branch);

  // Rebase on main
  git::Reference mainBranch = mRepo.lookupRef(QString("refs/heads/main"));
  QVERIFY(mainBranch.isValid());
  auto ac = mainBranch.annotatedCommit();
  LogEntry *entry = repoView->addLogEntry("Rebase", "Rebase", nullptr);
  repoView->rebase(ac, entry);

  QCOMPARE(mRepo.rebaseOngoing(), true);

  // Check that buttons are visible
  QTRY_COMPARE(continueRebaseButton->isVisible(), true);
  QTRY_COMPARE(rebaseBanner->isVisible(), true);

  // Resolve conflicts
  diff = mRepo.status(mRepo.index(), nullptr, false);
  QCOMPARE(diff.count(), 1);
  QCOMPARE(diff.patch(0).isConflicted(), true);
  QFile f(mRepo.workdir().filePath(diff.patch(0).name()));
  QCOMPARE(f.open(QIODevice::WriteOnly), true);
  QVERIFY(f.write("Test123") !=
          -1); // just write something to resolve the conflict
  f.close();

  repoView->continueRebase(); // should fail

  // Staging the file
  QTRY_COMPARE(repoView->findChildren<FileWidget *>().length(), 1);
  auto filewidgets = repoView->findChildren<FileWidget *>();

  // During a rebase, git's own "ours"/"theirs" are swapped relative to a
  // merge: "ours" is main, the branch being rebased onto, not the branch
  // ("singleCommitConflict") the user actually checked out and is rebasing.
  // The button labels must say so plainly rather than repeat that swap.
  QToolButton *ours =
      filewidgets.at(0)->findChild<QToolButton *>("ConflictFileOurs");
  QToolButton *theirs =
      filewidgets.at(0)->findChild<QToolButton *>("ConflictFileTheirs");
  QVERIFY(ours);
  QVERIFY(theirs);
  QCOMPARE(ours->text(), QString("Keep main"));
  QCOMPARE(theirs->text(), QString("Take singleCommitConflict"));

  filewidgets.at(0)->stageStateChanged(filewidgets.at(0)->modelIndex(),
                                       git::Index::StagedState::Staged);

  QTextEdit *editor = repoView->findChild<QTextEdit *>("MessageEditor");
  QVERIFY(editor);
  editor->setText("Test message"); // modify message

  repoView->continueRebase();

  // Check that branch is based on "main" now
  branch = mRepo.lookupRef(rebaseBranchName);
  QVERIFY(branch.isValid());
  QList<Commit> parents = branch.annotatedCommit().commit().parents();
  QCOMPARE(parents.count(), 1);
  QCOMPARE(parents.at(0).id(), ac.commit().id());
  QCOMPARE(branch.annotatedCommit().commit().message(),
           "Test message"); // Modified message is shown

  // Check that rebase was really finished
  QCOMPARE(mRepo.rebaseOngoing(), false);

  // Check that buttons are visible
  QTRY_COMPARE(continueRebaseButton->isVisible(), false);
  QTRY_COMPARE(rebaseBanner->isVisible(), false);
}

// A second handle on the repository has its own notifier, so the GUI only
// sees its changes after a refresh, like with an external tool.
void TestRebase::startExternalRebase(git::Repository &external) {
  git::Reference branch = external.lookupRef("refs/heads/singleCommitConflict");
  QVERIFY(branch.isValid());
  QVERIFY(external.checkout(branch.target()));
  QVERIFY(external.setHead(branch));

  git::Reference mainBranch = external.lookupRef("refs/heads/main");
  QVERIFY(mainBranch.isValid());
  external.rebase(mainBranch.annotatedCommit());
  QVERIFY(external.rebaseOngoing());
}

void TestRebase::startGuiRebase(RepoView *repoView) {
  git::Reference branch = mRepo.lookupRef("refs/heads/singleCommitConflict");
  QVERIFY(branch.isValid());
  repoView->checkout(branch);

  git::Reference mainBranch = mRepo.lookupRef("refs/heads/main");
  QVERIFY(mainBranch.isValid());
  LogEntry *entry = repoView->addLogEntry("Rebase", "Rebase", nullptr);
  repoView->rebase(mainBranch.annotatedCommit(), entry);
  QCOMPARE(mRepo.rebaseOngoing(), true);
}

void TestRebase::resolveConflict(git::Repository &repo) {
  auto status = repo.status(repo.index(), nullptr, false);
  QCOMPARE(status.count(), 1);
  QCOMPARE(status.patch(0).isConflicted(), true);

  const QString name = status.patch(0).name();
  QFile f(repo.workdir().filePath(name));
  QCOMPARE(f.open(QIODevice::WriteOnly), true);
  QVERIFY(f.write("Test123") != -1); // just write something to resolve it
  f.close();

  repo.index().setStaged({name}, true);
}

void TestRebase::continueExternalStartedRebase() {
  INIT_REPO("rebaseConflicts.zip");

  auto *detailview = repoView->findChild<DetailView *>();
  QVERIFY(detailview);
  auto *rebaseBanner = repoView->findChild<StateBanner *>();
  QVERIFY(rebaseBanner);
  auto *continueRebaseButton =
      detailview->findChild<QPushButton *>("ContinueRebase");
  QVERIFY(continueRebaseButton);
  QCOMPARE(continueRebaseButton->isVisible(), false);
  QCOMPARE(rebaseBanner->isVisible(), false);

  int rebaseFinished = 0;
  int rebaseAboutToRebase = 0;
  int rebaseCommitSuccess = 0;
  int rebaseConflict = 0;

  connect(mRepo.notifier(), &git::RepositoryNotifier::rebaseInitError,
          []() { QVERIFY(false); }); // Should not be called
  connect(mRepo.notifier(), &git::RepositoryNotifier::rebaseAboutToRebase,
          [&rebaseAboutToRebase]() { rebaseAboutToRebase++; });
  connect(mRepo.notifier(), &git::RepositoryNotifier::rebaseCommitInvalid,
          []() { QVERIFY(false); }); // Should not be called
  connect(mRepo.notifier(), &git::RepositoryNotifier::rebaseFinished,
          [&rebaseFinished]() { rebaseFinished++; });
  connect(mRepo.notifier(), &git::RepositoryNotifier::rebaseCommitSuccess,
          [&rebaseCommitSuccess]() { rebaseCommitSuccess++; });
  connect(mRepo.notifier(), &git::RepositoryNotifier::rebaseConflict,
          [&rebaseConflict]() { rebaseConflict++; });

  git::Repository external = git::Repository::open(path);
  QVERIFY(external.isValid());
  startExternalRebase(external);
  Test::refresh(repoView); // The GUI is not notified of external changes

  QTRY_COMPARE(continueRebaseButton->isVisible(), true);
  QTRY_COMPARE(rebaseBanner->isVisible(), true);

  resolveConflict(external);
  Test::refresh(repoView);

  repoView->continueRebase();

  // Check that branch is based on "main" now
  git::Reference branch = mRepo.lookupRef("refs/heads/singleCommitConflict");
  QVERIFY(branch.isValid());
  git::Reference mainBranch = mRepo.lookupRef("refs/heads/main");
  QVERIFY(mainBranch.isValid());
  QList<Commit> parents = branch.annotatedCommit().commit().parents();
  QCOMPARE(parents.count(), 1);
  QCOMPARE(parents.at(0).id(), mainBranch.annotatedCommit().commit().id());

  QCOMPARE(mRepo.rebaseOngoing(), false);
  QTRY_COMPARE(continueRebaseButton->isVisible(), false);
  QTRY_COMPARE(rebaseBanner->isVisible(), false);

  // Only the continue was done by the GUI
  QCOMPARE(rebaseFinished, 1);
  QCOMPARE(rebaseAboutToRebase, 0);
  QCOMPARE(rebaseCommitSuccess, 1);
  QCOMPARE(rebaseConflict, 0);
}

void TestRebase::startRebaseContinueExternally() {
  INIT_REPO("rebaseConflicts.zip");

  auto *detailview = repoView->findChild<DetailView *>();
  QVERIFY(detailview);
  auto *rebaseBanner = repoView->findChild<StateBanner *>();
  QVERIFY(rebaseBanner);
  auto *continueRebaseButton =
      detailview->findChild<QPushButton *>("ContinueRebase");
  QVERIFY(continueRebaseButton);

  int rebaseConflict = 0;
  connect(mRepo.notifier(), &git::RepositoryNotifier::rebaseInitError,
          []() { QVERIFY(false); }); // Should not be called
  connect(mRepo.notifier(), &git::RepositoryNotifier::rebaseCommitInvalid,
          []() { QVERIFY(false); }); // Should not be called
  connect(mRepo.notifier(), &git::RepositoryNotifier::rebaseConflict,
          [&rebaseConflict]() { rebaseConflict++; });

  startGuiRebase(repoView);
  QCOMPARE(rebaseConflict, 1);
  QTRY_COMPARE(continueRebaseButton->isVisible(), true);
  QTRY_COMPARE(rebaseBanner->isVisible(), true);

  git::Repository external = git::Repository::open(path);
  QVERIFY(external.isValid());
  resolveConflict(external);
  git::Rebase rebase = external.rebaseOpen();
  QVERIFY(rebase.isValid());
  external.rebaseContinue(rebase.commitToRebase().message());
  QCOMPARE(external.rebaseOngoing(), false);

  Test::refresh(repoView, false); // The GUI is not notified of external changes

  QCOMPARE(mRepo.rebaseOngoing(), false);
  QTRY_COMPARE(continueRebaseButton->isVisible(), false);
  QTRY_COMPARE(rebaseBanner->isVisible(), false);
}

void TestRebase::startRebaseContinueExternallyContinueGUI() {
  INIT_REPO("rebaseConflicts.zip");

  auto *detailview = repoView->findChild<DetailView *>();
  QVERIFY(detailview);
  auto *rebaseBanner = repoView->findChild<StateBanner *>();
  QVERIFY(rebaseBanner);
  auto *continueRebaseButton =
      detailview->findChild<QPushButton *>("ContinueRebase");
  QVERIFY(continueRebaseButton);

  int rebaseFinished = 0;
  int rebaseConflict = 0;
  connect(mRepo.notifier(), &git::RepositoryNotifier::rebaseInitError,
          []() { QVERIFY(false); }); // Should not be called
  connect(mRepo.notifier(), &git::RepositoryNotifier::rebaseCommitInvalid,
          []() { QVERIFY(false); }); // Should not be called
  connect(mRepo.notifier(), &git::RepositoryNotifier::rebaseFinished,
          [&rebaseFinished]() { rebaseFinished++; });
  connect(mRepo.notifier(), &git::RepositoryNotifier::rebaseConflict,
          [&rebaseConflict]() { rebaseConflict++; });

  startGuiRebase(repoView);
  QCOMPARE(rebaseFinished, 0);
  QCOMPARE(rebaseConflict, 1);
  QTRY_COMPARE(continueRebaseButton->isVisible(), true);
  QTRY_COMPARE(rebaseBanner->isVisible(), true);

  // Commit the conflicting step but leave finishing the rebase open
  git::Repository external = git::Repository::open(path);
  QVERIFY(external.isValid());
  resolveConflict(external);
  git::Rebase rebase = external.rebaseOpen();
  QVERIFY(rebase.isValid());
  QVERIFY(rebase.commit(rebase.commitToRebase().message()).isValid());
  QCOMPARE(external.rebaseOngoing(), true);

  Test::refresh(repoView, false); // The GUI is not notified of external changes

  QCOMPARE(mRepo.rebaseOngoing(), true);

  repoView->continueRebase();

  QCOMPARE(rebaseFinished, 1);
  QCOMPARE(mRepo.rebaseOngoing(), false);
  QTRY_COMPARE(continueRebaseButton->isVisible(), false);
  QTRY_COMPARE(rebaseBanner->isVisible(), false);
}

void TestRebase::abortMR() {
  INIT_REPO("rebaseConflicts.zip");

  auto *detailview = repoView->findChild<DetailView *>();
  QVERIFY(detailview);
  auto *rebaseBanner = repoView->findChild<StateBanner *>();
  QVERIFY(rebaseBanner);
  auto *continueRebaseButton =
      detailview->findChild<QPushButton *>("ContinueRebase");
  QVERIFY(continueRebaseButton);
  QCOMPARE(continueRebaseButton->isVisible(), false);
  QCOMPARE(rebaseBanner->isVisible(), false);

  int rebaseFinished = 0;
  int rebaseAboutToRebase = 0;
  int rebaseCommitSuccess = 0;
  int rebaseConflict = 0;
  int refreshTriggered = 0;

  connect(mRepo.notifier(), &git::RepositoryNotifier::rebaseInitError,
          []() { QVERIFY(false); }); // Should not be called
  connect(mRepo.notifier(), &git::RepositoryNotifier::rebaseAboutToRebase,
          [&rebaseAboutToRebase](const Rebase rebase, const Commit before,
                                 int count) {
            QVERIFY(rebase.isValid());
            QCOMPARE(count, 1);
            QCOMPARE(before.message(), "File.txt changed by second branch\n");
            rebaseAboutToRebase++;
          });
  connect(mRepo.notifier(), &git::RepositoryNotifier::rebaseCommitInvalid,
          []() { QVERIFY(false); }); // Should not be called
  connect(mRepo.notifier(), &git::RepositoryNotifier::rebaseFinished,
          [&rebaseFinished]() { rebaseFinished++; });
  connect(mRepo.notifier(), &git::RepositoryNotifier::rebaseCommitSuccess,
          [&rebaseCommitSuccess](const Rebase rebase, const Commit before,
                                 const Commit after, int counter) {
            QVERIFY(rebase.isValid());
            rebaseCommitSuccess++;
          });
  connect(mRepo.notifier(), &git::RepositoryNotifier::rebaseConflict,
          [&rebaseConflict, &rebaseCommitSuccess]() {
            QCOMPARE(rebaseCommitSuccess, 0); // was not called yet
            rebaseConflict++;
          });

  connect(mRepo.notifier(), &RepositoryNotifier::referenceUpdated,
          [this, &refreshTriggered](const Reference &ref) {
            // TODO: enable
            // QCOMPARE(ref, mRepo.head());
            refreshTriggered++;
          });

  const QString rebaseBranchName = "refs/heads/singleCommitConflict";

  git::Reference branch = mRepo.lookupRef(rebaseBranchName);
  QVERIFY(branch.isValid());
  auto c = branch.annotatedCommit().commit();

  // Checkout correct branch
  repoView->checkout(branch);

  // Rebase on main
  git::Reference mainBranch = mRepo.lookupRef(QString("refs/heads/main"));
  QVERIFY(mainBranch.isValid());
  auto ac = mainBranch.annotatedCommit();
  refreshTriggered = 0;
  LogEntry *entry = repoView->addLogEntry("Rebase", "Rebase", nullptr);
  repoView->rebase(ac, entry);
  QCOMPARE(refreshTriggered, 1); // Check that refresh was triggered

  QCOMPARE(mRepo.rebaseOngoing(), true);
  QCOMPARE(rebaseFinished, 0);
  QCOMPARE(rebaseConflict, 1);

  // Check that buttons are visible
  QTRY_COMPARE(continueRebaseButton->isVisible(), true);
  QTRY_COMPARE(rebaseBanner->isVisible(), true);

  refreshTriggered = 0;
  rebaseConflict = 0;
  repoView->abortRebase();
  QCOMPARE(refreshTriggered, 1);

  // Check that rebase was really finished
  QCOMPARE(mRepo.rebaseOngoing(), false);

  // Check that buttons are visible
  QTRY_COMPARE(continueRebaseButton->isVisible(), false);
  QTRY_COMPARE(rebaseBanner->isVisible(), false);

  // Check call counters
  QCOMPARE(rebaseFinished, 0);
  QCOMPARE(rebaseAboutToRebase, 1);
  QCOMPARE(rebaseCommitSuccess, 0);
  QCOMPARE(rebaseConflict, 0);
}

void TestRebase::commitDuringRebase() {
  /*
   * Start rebasing in gui
   * Solve conflicts
   * Commit instead of continue rebase
   * Commit something else too
   * Continue rebase */

  INIT_REPO("rebaseConflicts.zip");

  auto *detailview = repoView->findChild<DetailView *>();
  QVERIFY(detailview);
  auto *rebaseBanner = repoView->findChild<StateBanner *>();
  QVERIFY(rebaseBanner);
  auto *continueRebaseButton =
      detailview->findChild<QPushButton *>("ContinueRebase");
  QVERIFY(continueRebaseButton);
  QCOMPARE(continueRebaseButton->isVisible(), false);
  QCOMPARE(rebaseBanner->isVisible(), false);

  int rebaseFinished = 0;
  int rebaseAboutToRebase = 0;
  int rebaseCommitSuccess = 0;
  int rebaseConflict = 0;
  int refreshTriggered = 0;

  connect(mRepo.notifier(), &git::RepositoryNotifier::rebaseInitError,
          []() { QVERIFY(false); }); // Should not be called
  connect(mRepo.notifier(), &git::RepositoryNotifier::rebaseAboutToRebase,
          [&rebaseAboutToRebase](const Rebase rebase, const Commit before,
                                 int count) {
            QVERIFY(rebase.isValid());
            QCOMPARE(count, 1);
            QCOMPARE(before.message(), "File.txt changed by second branch\n");
            rebaseAboutToRebase++;
          });
  connect(mRepo.notifier(), &git::RepositoryNotifier::rebaseCommitInvalid,
          []() { QVERIFY(false); }); // Should not be called
  connect(mRepo.notifier(), &git::RepositoryNotifier::rebaseFinished,
          [&rebaseFinished]() { rebaseFinished++; });
  connect(mRepo.notifier(), &git::RepositoryNotifier::rebaseCommitSuccess,
          [&rebaseCommitSuccess](const Rebase rebase, const Commit before,
                                 const Commit after, int counter) {
            QVERIFY(rebase.isValid());
            rebaseCommitSuccess++;
          });
  connect(mRepo.notifier(), &git::RepositoryNotifier::rebaseConflict,
          [&rebaseConflict, &rebaseCommitSuccess]() {
            QCOMPARE(rebaseCommitSuccess, 0); // was not called yet
            rebaseConflict++;
          });

  connect(mRepo.notifier(), &RepositoryNotifier::referenceUpdated,
          [this, &refreshTriggered](const Reference &ref) {
            // TODO: enable
            // QCOMPARE(ref, mRepo.head());
            refreshTriggered++;
          });

  const QString rebaseBranchName = "refs/heads/singleCommitConflict";

  git::Reference branch = mRepo.lookupRef(rebaseBranchName);
  QVERIFY(branch.isValid());
  auto c = branch.annotatedCommit().commit();

  // Checkout correct branch
  repoView->checkout(branch);

  // Rebase on main
  git::Reference mainBranch = mRepo.lookupRef(QString("refs/heads/main"));
  QVERIFY(mainBranch.isValid());
  auto ac = mainBranch.annotatedCommit();
  refreshTriggered = 0;
  LogEntry *entry = repoView->addLogEntry("Rebase", "Rebase", nullptr);
  repoView->rebase(ac, entry);
  QCOMPARE(refreshTriggered, 1); // Check that refresh was triggered

  QCOMPARE(mRepo.rebaseOngoing(), true);
  QCOMPARE(rebaseFinished, 0);
  QCOMPARE(rebaseConflict, 1);

  // Check that buttons are visible
  QTRY_COMPARE(continueRebaseButton->isVisible(), true);
  QTRY_COMPARE(rebaseBanner->isVisible(), true);

  // Resolve conflicts
  diff = mRepo.status(mRepo.index(), nullptr, false);
  QCOMPARE(diff.count(), 1);
  QCOMPARE(diff.patch(0).isConflicted(), true);
  QFile f(mRepo.workdir().filePath(diff.patch(0).name()));
  QCOMPARE(f.open(QIODevice::WriteOnly), true);
  QVERIFY(f.write("Test123") !=
          -1); // just write something to resolve the conflict
  f.close();

  Test::refresh(repoView);

  // stage file otherwise it is not possible to continue
  auto filewidgets = repoView->findChildren<FileWidget *>();
  QCOMPARE(filewidgets.length(), 1);
  filewidgets.at(0)->stageStateChanged(filewidgets.at(0)->modelIndex(),
                                       git::Index::StagedState::Staged);

  QTextEdit *editor = repoView->findChild<QTextEdit *>("MessageEditor");
  QVERIFY(editor);
  editor->setText("Test message");

  // Do commit before going on
  // So the user can commit between the rebase to split up the changes
  bool force = true;
  repoView->commit(force);

  refreshTriggered = 0;
  rebaseConflict = 0;
  repoView->continueRebase();
  QCOMPARE(refreshTriggered, 1);

  // Check that branch is based on "main" now
  branch = mRepo.lookupRef(rebaseBranchName);
  QVERIFY(branch.isValid());
  QList<Commit> parents = branch.annotatedCommit().commit().parents();
  QCOMPARE(parents.count(), 1);
  QCOMPARE(parents.at(0).id(), ac.commit().id());
  QCOMPARE(
      branch.annotatedCommit().commit().message(),
      "Test message"); // custom message was used instead of the original one

  // Check that rebase was really finished
  QCOMPARE(mRepo.rebaseOngoing(), false);

  // Check that buttons are visible
  QTRY_COMPARE(continueRebaseButton->isVisible(), false);
  QTRY_COMPARE(rebaseBanner->isVisible(), false);

  // Check call counters
  QCOMPARE(rebaseFinished, 1);
  QCOMPARE(rebaseAboutToRebase, 1);
  QCOMPARE(rebaseCommitSuccess, 1);
  QCOMPARE(rebaseConflict, 0);
}

TEST_MAIN(TestRebase)

#include "rebase.moc"
