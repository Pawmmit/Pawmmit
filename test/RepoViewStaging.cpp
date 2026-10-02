#include "Test.h"

#include "ui/detail/DoubleTreeWidget.h"
#include "ui/filetree/TreeView.h"
#include "ui/repo/RepoView.h"
#include "ui/window/MainWindow.h"
#include "watcher/RepositoryWatcher.h"

#include <QSignalSpy>
#include <memory>

using namespace Test;

namespace {

const int kDebounceMs = 100;

// Timeouts are generous so a starved CI runner can't cause false failures.
const int kAttemptMs = 1500;
const int kAttempts = 10;
const int kSignalMs = 10000;

bool writeFile(const QDir &workdir, const QString &path,
               const QByteArray &data) {
  QFile file(workdir.filePath(path));
  return file.open(QFile::WriteOnly | QFile::Truncate) &&
         file.write(data) == data.size();
}

RepositoryWatcher *findWatcher(RepoView *view) {
  for (QObject *child : view->children()) {
    if (auto *watcher = dynamic_cast<RepositoryWatcher *>(child))
      return watcher;
  }

  return nullptr;
}

// Name of the selected file, like DoubleTreeWidget remembers it.
QString selectedFile(DoubleTreeWidget *trees) {
  for (const char *name : {"Staged", "Unstaged"}) {
    auto *tree = trees->findChild<TreeView *>(name);
    QModelIndexList indexes = tree->selectionModel()->selectedIndexes();
    if (!indexes.isEmpty())
      return indexes.first().data(Qt::EditRole).toString();
  }

  return QString();
}

} // namespace

class TestRepoViewStaging : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void partialStagingKeepsSelection();
  void cleanupTestCase();

private:
  ScratchRepository mRepo;
  MainWindow *mWindow = nullptr;
  RepoView *mView = nullptr;
  std::unique_ptr<QSignalSpy> mSpy;
};

void TestRepoViewStaging::initTestCase() {
  QDir workdir = mRepo->workdir();
  QVERIFY(writeFile(workdir, "a.txt", "a1\na2\na3\n"));
  QVERIFY(writeFile(workdir, "b.txt", "b1\nb2\nb3\n"));

  mWindow = new MainWindow(mRepo);
  mWindow->show();
  QVERIFY(QTest::qWaitForWindowActive(mWindow));
  mView = mWindow->currentView();

  RepositoryWatcher *watcher = findWatcher(mView);
  QVERIFY(watcher);
  watcher->setDebounceInterval(kDebounceMs);

  mSpy = std::make_unique<QSignalSpy>(mRepo->notifier(),
                                      &git::RepositoryNotifier::workdirChanged);

  // The watcher installs its watches on a thread, so retry until it reports.
  bool watching = false;
  for (int i = 0; i < kAttempts && !watching; ++i) {
    QVERIFY(writeFile(workdir, "canary", "x"));
    watching = mSpy->wait(kAttemptMs);
  }

  QVERIFY2(watching, "watcher never reported a root-level change");
  QTest::qWait(2 * kDebounceMs);
}

void TestRepoViewStaging::partialStagingKeepsSelection() {
  refresh(mView);

  auto *trees = mView->findChild<DoubleTreeWidget *>();
  QVERIFY(trees);
  auto *unstaged = trees->findChild<TreeView *>("Unstaged");
  QVERIFY(unstaged);

  // Select the second file.
  QModelIndex b;
  for (int row = 0; row < unstaged->model()->rowCount(); ++row) {
    QModelIndex index = unstaged->model()->index(row, 0);
    if (index.data(Qt::EditRole).toString() == "b.txt")
      b = index;
  }

  QVERIFY(b.isValid());
  unstaged->selectionModel()->setCurrentIndex(
      b, QItemSelectionModel::ClearAndSelect);
  QCOMPARE(selectedFile(trees), QString("b.txt"));

  // Stage part of it the way FileWidget::stageHunks does: only the index is
  // written, and the UI isn't refreshed.
  mSpy->clear();
  mRepo->index().add("b.txt", "b1\nb2\n");

  // Let whatever refresh follows run to completion.
  QTest::qWait(3 * kDebounceMs);
  QTRY_VERIFY_WITH_TIMEOUT(!mView->isLoading(), kSignalMs);
  QTest::qWait(kDebounceMs);

  qInfo() << "workdirChanged emitted:" << mSpy->count()
          << "selected:" << selectedFile(trees);
  QCOMPARE(selectedFile(trees), QString("b.txt"));
}

void TestRepoViewStaging::cleanupTestCase() {
  mSpy.reset();
  mWindow->close();

  // Delete the window now so its watcher stops before the repository goes.
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

TEST_MAIN(TestRepoViewStaging)
#include "RepoViewStaging.moc"
