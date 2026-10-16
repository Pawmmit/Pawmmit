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
#include "ui/commits/CommitList.h"
#include "ui/detail/DetailView.h"
#include "ui/detail/DoubleTreeWidget.h"
#include "ui/diffView/HunkWidget.h"
#include "ui/filetree/TreeView.h"
#include "ui/repo/RepoView.h"
#include "ui/window/MainWindow.h"
#include <QMessageBox>
#include <QPushButton>
#include <QToolButton>

using namespace Test;
using namespace QTest;

class TestLineEndings : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void createFile();
  void commitFile();
  void overwriteFile();
  void testLineEndings();
  void discardHunkKeepsCrlf();
  void cleanupTestCase();

private:
  int inputDelay = 0;
  int closeDelay = 0;

  ScratchRepository mRepo;
  MainWindow *mWindow = nullptr;
  RepoView *mRepoView = nullptr;
};

void TestLineEndings::initTestCase() {
  mWindow = new MainWindow(mRepo);
  mWindow->show();
  QVERIFY(qWaitForWindowActive(mWindow));
  mRepoView = mWindow->currentView();
}

void TestLineEndings::createFile() {
  QDir workingDirectory = mRepo->workdir();
  QFile mFile(workingDirectory.filePath("test_file"));
  QVERIFY(mFile.open(QFile::WriteOnly));
  QTextStream(&mFile) << "git config --global core.autocrlf true";
  mFile.close();
  refresh(mRepoView);
}

void TestLineEndings::commitFile() {
  DetailView *detailView = mRepoView->findChild<DetailView *>();
  detailView->setCommitMessage(
      "Discarding file should not leave behind CRLF remnants on Windows");
  QList<QPushButton *> buttons = detailView->findChildren<QPushButton *>();
  QPushButton *stageAll = buttons.at(0);
  QPushButton *commit = buttons.at(2);
  mouseClick(stageAll, Qt::LeftButton, Qt::KeyboardModifiers(), QPoint(),
             inputDelay);
  mouseClick(commit, Qt::LeftButton, Qt::KeyboardModifiers(), QPoint(),
             inputDelay);
}

void TestLineEndings::overwriteFile() {
  QDir workingDirectory = mRepo->workdir();
  QFile file(workingDirectory.filePath("test_file"));
  QVERIFY(file.open(QFile::WriteOnly));
  QTextStream(&file) << "\n This change will be discarded";
  file.close();
  refresh(mRepoView);
}

void TestLineEndings::testLineEndings() {
  auto doubleTree = mRepoView->findChild<DoubleTreeWidget *>();
  QVERIFY(doubleTree);

  auto files = doubleTree->findChild<TreeView *>("Unstaged");
  QVERIFY(files);

  files->selectionModel()->select(files->model()->index(0, 0),
                                  QItemSelectionModel::Select);

  int inputDelay = this->inputDelay;
  auto mRepoView = this->mRepoView;
  QTimer::singleShot(100, [inputDelay, mRepoView] {
    QMessageBox *popup = mRepoView->findChild<QMessageBox *>();
    QVERIFY(popup);

    QList<QAbstractButton *> buttons = popup->buttons();
    QAbstractButton *accept = buttons.at(0);
    QVERIFY(accept);

    mouseClick(accept, Qt::LeftButton, Qt::KeyboardModifiers(), QPoint(),
               inputDelay);
  });

  QToolButton *button = mRepoView->findChild<QToolButton *>("DiscardButton");
  mouseClick(button, Qt::LeftButton, Qt::KeyboardModifiers(), QPoint(),
             inputDelay);

  CommitList *commitList = mRepoView->findChild<CommitList *>();
  QVERIFY(!commitList->status().isValid());
}

void TestLineEndings::discardHunkKeepsCrlf() {
  ScratchRepository repo;
  QDir workdir = repo->workdir();

  auto lines = [](const QList<int> &changed) {
    QByteArray text;
    for (int i = 1; i <= 20; ++i)
      text += QByteArray("line ") + QByteArray::number(i) +
              (changed.contains(i) ? " changed" : "") + "\r\n";
    return text;
  };

  auto write = [&workdir](const QString &name, const QByteArray &data) {
    QFile file(workdir.filePath(name));
    return file.open(QFile::WriteOnly) && file.write(data) == data.size();
  };

  QVERIFY(write(".gitattributes", "file.txt text eol=crlf\n"));
  QVERIFY(write("file.txt", lines({})));
  repo->index().setStaged({".gitattributes", "file.txt"}, true);
  QVERIFY(repo->commit("Add CRLF file").isValid());

  // Two changes far enough apart to form separate hunks.
  QVERIFY(write("file.txt", lines({2, 19})));

  MainWindow window(repo);
  window.show();
  QVERIFY(qWaitForWindowActive(&window));
  RepoView *view = window.currentView();
  refresh(view);

  auto files =
      view->findChild<DoubleTreeWidget *>()->findChild<TreeView *>("Unstaged");
  QVERIFY(files);
  files->selectionModel()->select(files->model()->index(0, 0),
                                  QItemSelectionModel::Select);

  QList<HunkWidget *> hunks;
  QTRY_VERIFY((hunks = view->findChildren<HunkWidget *>()).size() == 2);

  // Discard the first hunk; the change on line 19 stays.
  HunkWidget *first = hunks.first();
  first->discardSelected(0, first->editor()->lineCount());

  QFile file(workdir.filePath("file.txt"));
  QVERIFY(file.open(QFile::ReadOnly));
  QCOMPARE(file.readAll(), lines({19}));
}

void TestLineEndings::cleanupTestCase() {
  qWait(closeDelay);
  mWindow->close();
}

TEST_MAIN(TestLineEndings)

#include "line_endings.moc"
