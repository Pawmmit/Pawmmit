#include "Test.h"

#include "git/Index.h"
#include "ui/commits/CommitList.h"
#include "ui/detail/DoubleTreeWidget.h"
#include "ui/filetree/TreeView.h"
#include "ui/repo/RepoView.h"
#include "ui/window/MainWindow.h"
#include "watcher/RepositoryWatcher.h"

#include <QSignalSpy>
#include <memory>

using namespace Test;

namespace {

const int kDebounceMs = 300;

// Long enough for the watcher to arm its timer, short of the debounce.
const int kArmMs = 100;

// Late notifications from earlier changes must land before the spy is cleared.
const int kSettleMs = 2 * kDebounceMs;

// Long enough for a spurious notification to show up.
const int kQuietMs = 3 * kDebounceMs;

// Timeouts are generous so a starved CI runner can't cause false failures.
const int kAttemptMs = 1500;
const int kAttempts = 10;
const int kSignalMs = 10000;

bool writeFile(const QString &path) {
  static int counter = 0;
  QFile file(path);
  if (!file.open(QFile::WriteOnly | QFile::Truncate))
    return false;

  file.write(QByteArray::number(++counter));
  return true;
}

RepositoryWatcher *findWatcher(RepoView *view) {
  for (QObject *child : view->children()) {
    if (auto *watcher = dynamic_cast<RepositoryWatcher *>(child))
      return watcher;
  }

  return nullptr;
}

} // namespace

class TestRepoViewAutoRefresh : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void changeSurvivesStatusFinishing();
  void refreshDoesNotRetrigger();
  void refreshDoesNotStealFocus();
  void refreshKeepsCommittedFileSelection();
  void cleanupTestCase();

private:
  void settle();

  ScratchRepository mRepo;
  MainWindow *mWindow = nullptr;
  RepoView *mView = nullptr;
  std::unique_ptr<QSignalSpy> mSpy;
};

void TestRepoViewAutoRefresh::settle() {
  QTest::qWait(kSettleMs);
  mSpy->clear();
}

void TestRepoViewAutoRefresh::initTestCase() {
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
    QVERIFY(writeFile(mRepo->workdir().filePath("canary")));
    watching = mSpy->wait(kAttemptMs);
  }

  QVERIFY2(watching, "watcher never reported a root-level change");
  settle();
}

void TestRepoViewAutoRefresh::changeSurvivesStatusFinishing() {
  QVERIFY(writeFile(mRepo->workdir().filePath("race")));
  QTest::qWait(kArmMs);

  // A finishing status check must not swallow a change made while it ran.
  auto *commits = mView->findChild<CommitList *>();
  QVERIFY(commits);
  emit commits->statusChanged(true);

  QVERIFY2(mSpy->wait(kSignalMs), "pending change was cancelled");
}

void TestRepoViewAutoRefresh::refreshDoesNotRetrigger() {
  settle();
  refresh(mView);

  mSpy->clear();
  QVERIFY2(!mSpy->wait(kQuietMs), "a refresh triggered another refresh");
}

void TestRepoViewAutoRefresh::refreshDoesNotStealFocus() {
  settle();

  // A refresh must not steal focus from the files tree.
  auto *doubleTree = mView->findChild<DoubleTreeWidget *>();
  QVERIFY(doubleTree);
  auto *unstagedFiles = doubleTree->findChild<TreeView *>("Unstaged");
  QVERIFY(unstagedFiles);

  unstagedFiles->setFocus();
  QVERIFY(unstagedFiles->hasFocus());

  refresh(mView);

  QVERIFY2(unstagedFiles->hasFocus(),
           "refresh moved keyboard focus away from the files tree");
}

void TestRepoViewAutoRefresh::refreshKeepsCommittedFileSelection() {
  settle();

  // Commit a file so there's a historical commit to browse.
  QString name = "committed.txt";
  QFile file(mRepo->workdir().filePath(name));
  QVERIFY(file.open(QFile::WriteOnly));
  file.write("content");
  file.close();
  mRepo->index().setStaged({name}, true);
  QVERIFY(mRepo->commit("add committed file"));
  settle();

  auto *commits = mView->findChild<CommitList *>();
  QVERIFY(commits);
  QTRY_VERIFY_WITH_TIMEOUT(commits->model()->rowCount() >= 1, 10000);
  commits->selectionModel()->select(commits->model()->index(0, 0),
                                    QItemSelectionModel::ClearAndSelect);
  QTRY_VERIFY_WITH_TIMEOUT(mView->diff().isValid(), 10000);

  auto *doubleTree = mView->findChild<DoubleTreeWidget *>();
  QVERIFY(doubleTree);
  auto *unstagedFiles = doubleTree->findChild<TreeView *>("Unstaged");
  QVERIFY(unstagedFiles);
  QTRY_VERIFY_WITH_TIMEOUT(unstagedFiles->model()->rowCount() >= 1, 10000);
  unstagedFiles->selectionModel()->select(unstagedFiles->model()->index(0, 0),
                                          QItemSelectionModel::ClearAndSelect);

  refresh(mView);

  QVERIFY2(!unstagedFiles->selectionModel()->selectedIndexes().isEmpty(),
           "refresh lost the selection in the committed files list");
}

void TestRepoViewAutoRefresh::cleanupTestCase() {
  mSpy.reset();
  mWindow->close();

  // Delete the window now so its watcher stops before the repository goes.
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

TEST_MAIN(TestRepoViewAutoRefresh)
#include "RepoViewAutoRefresh.moc"
