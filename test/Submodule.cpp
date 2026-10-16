//
//          Copyright (c) 2022, Pawmmit Team
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

#include "conf/Settings.h"
#include "dialogs/CloneDialog.h"
#include "git/Submodule.h"
#include "qtsupport.h"
#include "ui/detail/DoubleTreeWidget.h"
#include "ui/filetree/TreeView.h"
#include "ui/repo/RepoView.h"
#include "ui/window/MainWindow.h"

#include <QLineEdit>
#include <QMenu>
#include <QToolButton>
#include <QWizard>

#define INIT_REPO(repoPath)                                                    \
  QString path = Test::extractRepository(repoPath);                            \
  QVERIFY(!path.isEmpty());                                                    \
  auto repo = git::Repository::open(path);                                     \
  QVERIFY(repo.isValid());                                                     \
  Test::initRepo(repo);                                                        \
  MainWindow window(repo);                                                     \
  window.show();                                                               \
  QVERIFY(QTest::qWaitForWindowExposed(&window));                              \
                                                                               \
  RepoView *repoView = window.currentView();                                   \
  auto diff = repo.status(repo.index(), nullptr, false);

using namespace Test;
using namespace QTest;

class TestSubmodule : public QObject {
  Q_OBJECT

private slots:
  void updateSubmoduleClone();
  void noUpdateSubmoduleClone();
  void discardFile();

private:
};

void TestSubmodule::updateSubmoduleClone() {
  // Update submodules after cloning
  QString remote = Test::extractRepository("SubmoduleTest.zip");
  QCOMPARE(remote.isEmpty(), false);

  Settings *settings = Settings::instance();
  settings->setValue(Setting::Id::UpdateSubmodulesAfterPullAndClone, true);
  CloneDialog *d = new CloneDialog(CloneDialog::Kind::Clone);

  RepoView *view = nullptr;
  MainWindow *window = nullptr;

  bool cloneFinished = false;
  QObject::connect(d, &CloneDialog::accepted,
                   [d, &window, &view, &cloneFinished] {
                     cloneFinished = true;
                     window = MainWindow::open(d->path());
                     if (window) {
                       view = window->currentView();
                     }
                   });

  QTemporaryDir tempdir;
  QVERIFY(tempdir.isValid());
  d->setField("url", remote);
  d->setField("name", "TestrepoSubmodule");
  d->setField("path", tempdir.path());
  d->setField("bare", "false");
  d->page(2)->initializePage(); // start clone

  QTRY_VERIFY_WITH_TIMEOUT(cloneFinished, 10000);

  QVERIFY(view);
  QCOMPARE(view->repo().submodules().count(), 1);
  for (const auto &s : view->repo().submodules()) {
    QVERIFY(s.isValid());
    QVERIFY(s.isInitialized());
  }

  // Close the window (and its tabs) now, before tempdir's destructor below
  // deletes the cloned repo out from under it -- otherwise it lingers as a
  // dangling tab that later tests' sidebar refreshes can trip over. Window
  // actually gone before this function (and tempdir) returns.
  // deleted.
  window->close();
  qWait(0);
}

void TestSubmodule::noUpdateSubmoduleClone() {
  // Don't update submodules after cloning
  QString remote = Test::extractRepository("SubmoduleTest.zip");
  QCOMPARE(remote.isEmpty(), false);

  Settings *settings = Settings::instance();
  settings->setValue(Setting::Id::UpdateSubmodulesAfterPullAndClone, false);
  CloneDialog *d = new CloneDialog(CloneDialog::Kind::Clone);

  RepoView *view = nullptr;
  MainWindow *window = nullptr;

  bool cloneFinished = false;
  QObject::connect(d, &CloneDialog::accepted,
                   [d, &window, &view, &cloneFinished] {
                     cloneFinished = true;
                     window = MainWindow::open(d->path());
                     if (window) {
                       view = window->currentView();
                     }
                   });

  QTemporaryDir tempdir;
  QVERIFY(tempdir.isValid());
  d->setField("url", remote);
  d->setField("name", "TestrepoSubmodule");
  d->setField("path", tempdir.path());
  d->setField("bare", "false");
  d->page(2)->initializePage(); // start clone

  QTRY_VERIFY_WITH_TIMEOUT(cloneFinished, 10000);

  QVERIFY(view);
  QCOMPARE(view->repo().submodules().count(), 1);
  for (const auto &s : view->repo().submodules()) {
    QVERIFY(s.isValid());
    QCOMPARE(s.isInitialized(), false);
  }

  // Close the window (and its tabs) now, before tempdir's destructor below
  // updateSubmoduleClone().
  window->close();
  qWait(0); // let the WA_DeleteOnClose deferred deletion run now
}

void TestSubmodule::discardFile() {
  // Discarding a file should not reset the submodule
  INIT_REPO("SubmoduleTest.zip");
  repoView->updateSubmodules(repo.submodules(), true, true);

  // The update also starts a refresh, which must finish before files change.
  QTRY_VERIFY_WITH_TIMEOUT(!repoView->isBusy() && !repoView->isLoading(),
                           10000);

  QCOMPARE(repo.submodules().count(), 1);
  for (const auto &submodule : repo.submodules())
    QVERIFY(submodule.isInitialized());

  {
    QFile file(repo.workdir().filePath("README.md"));
    QVERIFY(file.open(QFile::WriteOnly));
    QTextStream(&file) << "Changing readme of main repository" << Qt::endl;
    file.close();
  }

  {
    QFile file(repo.workdir().filePath("GittyupTestRepo/README.md"));
    QVERIFY(file.open(QFile::WriteOnly));
    QTextStream(&file) << "Changing content of submodule readme" << Qt::endl;
    file.close();
  }

  auto doubleTree = repoView->findChild<DoubleTreeWidget *>();
  QVERIFY(doubleTree);

  // Select head
  // Does not work
  // repoView->selectHead();
  // repoView->selectFirstCommit();

  refresh(repoView); // Do a refresh to simulate selecting the working directory
                     // entry in the commit list

  {
    // wait for refresh!
    auto unstagedTree = doubleTree->findChild<TreeView *>("Unstaged");
    QVERIFY(unstagedTree);
    QAbstractItemModel *unstagedModel = unstagedTree->model();
    QTRY_VERIFY_WITH_TIMEOUT(
        unstagedModel->rowCount() >= 2 &&
            unstagedModel->data(unstagedModel->index(1, 0)) == "README.md",
        10000);
  }

  {
    auto unstagedTree = doubleTree->findChild<TreeView *>("Unstaged");
    QVERIFY(unstagedTree);
    QAbstractItemModel *unstagedModel = unstagedTree->model();

    QCOMPARE(unstagedModel->rowCount(), 2);
    unstagedModel->index(0, 0);
    auto readme = unstagedModel->index(1, 0);
    QCOMPARE(unstagedModel->data(readme).toString(), QString("README.md"));

    unstagedTree->discard(readme, true);
  }

  QFile file(repo.workdir().filePath("GittyupTestRepo/README.md"));
  QVERIFY(file.open(QFile::ReadOnly));
  QCOMPARE(file.readAll(), "Changing content of submodule readme\n");
}

TEST_MAIN(TestSubmodule)

#include "Submodule.moc"
