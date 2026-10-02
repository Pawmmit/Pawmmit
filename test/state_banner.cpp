#include "Test.h"
#include "conf/Settings.h"
#include "ui/repo/RepoView.h"
#include "ui/repo/StateBanner.h"
#include "ui/window/MainWindow.h"
#include "ui/window/ToolBar.h"
#include <QAbstractButton>
#include <QLabel>
#include <QPushButton>
#include <QStatusBar>
#include <QTranslator>

using namespace Test;
using namespace QTest;

class TestStateBanner : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void quietOnABranch();
  void statusBarExplainsTheUpstream();
  void countsAreInWords();
  void warnsAboutADetachedHead();
  void offersTheWayBack();
  void goesAwayOnABranchAgain();
  void cleanupTestCase();

private:
  StateBanner *banner() const { return mView->findChild<StateBanner *>(); }

  // The banner's current buttons, ignoring ones about to be deleted.
  QList<QPushButton *> buttons() const {
    QList<QPushButton *> result;
    for (QPushButton *button : banner()->findChildren<QPushButton *>()) {
      if (button->isVisibleTo(banner()))
        result.append(button);
    }
    return result;
  }

  QStringList buttonTexts() const {
    QStringList texts;
    for (QPushButton *button : buttons())
      texts.append(button->text());
    return texts;
  }

  QPushButton *button(const QString &text) const {
    for (QPushButton *button : buttons()) {
      if (button->text() == text)
        return button;
    }
    return nullptr;
  }

  QLabel *label(const char *name) const {
    return mWindow->statusBar()->findChild<QLabel *>(name);
  }

  git::Repository mRepo;
  MainWindow *mWindow = nullptr;
  RepoView *mView = nullptr;
  QString mBranch;
};

void TestStateBanner::initTestCase() {
  QString path = extractRepository("rebaseConflicts.zip");
  QVERIFY(!path.isEmpty());
  mRepo = git::Repository::open(path);
  QVERIFY(mRepo.isValid());
  initRepo(mRepo);

  mWindow = new MainWindow(mRepo);
  mWindow->show();
  QVERIFY(qWaitForWindowExposed(mWindow));

  mView = mWindow->currentView();
  QVERIFY(mView);
  mBranch = mRepo.head().name();
}

void TestStateBanner::quietOnABranch() {
  QVERIFY(banner());
  QVERIFY(!banner()->isVisible());

  // The status bar names the branch instead, and says it has no upstream.
  QCOMPARE(label("BranchLabel")->text(), "On branch " + mBranch);
  QCOMPARE(label("UpstreamLabel")->text(), QString("only on this computer"));
  QVERIFY(!label("UpstreamLabel")->toolTip().isEmpty());
  QVERIFY(label("SyncLabel")->isHidden());
}

void TestStateBanner::statusBarExplainsTheUpstream() {
  git::Branch branch = mRepo.head();
  git::Branch upstream = mRepo.lookupBranch("main", GIT_BRANCH_LOCAL);
  QVERIFY(branch.isValid() && upstream.isValid());
  branch.setUpstream(upstream);

  int ahead = branch.difference(upstream);
  int behind = upstream.difference(branch);
  QVERIFY(ahead + behind > 0);

  emit mRepo.notifier()->referenceUpdated(mRepo.head());
  QTRY_COMPARE(label("UpstreamLabel")->text(), QString("linked to main"));

  QStringList parts;
  if (ahead > 0)
    parts.append(MainWindow::commitsToPush(ahead));
  if (behind > 0)
    parts.append(MainWindow::commitsToPull(behind));
  QCOMPARE(label("SyncLabel")->text(), parts.join(", "));
  QVERIFY(label("SyncLabel")->toolTip().contains("last fetch"));

  // The tool bar badges say the same in words.
  for (QAbstractButton *button :
       mWindow->toolBar()->findChildren<QAbstractButton *>()) {
    if (ahead > 0 && button->accessibleName() == "Push")
      QVERIFY(button->toolTip().contains(MainWindow::commitsToPush(ahead)));
    if (behind > 0 && button->accessibleName() == "Pull")
      QVERIFY(button->toolTip().contains(MainWindow::commitsToPull(behind)));
  }
}

void TestStateBanner::countsAreInWords() {
  // Tests run untranslated, but English needs its plural forms from l10n.
  QTranslator english;
  QVERIFY(english.load("pawmmit_en", Settings::l10nDir().absolutePath()));
  QCoreApplication::installTranslator(&english);

  QCOMPARE(MainWindow::commitsToPush(1), QString("1 commit to push"));
  QCOMPARE(MainWindow::commitsToPush(3), QString("3 commits to push"));
  QCOMPARE(MainWindow::commitsToPull(1), QString("1 commit to pull"));
  QCOMPARE(MainWindow::commitsToPull(2), QString("2 commits to pull"));

  const char *conflicts = "%n file(s) have conflicts. Keep one version or "
                          "edit it, then stage it to mark it resolved.";
  QCOMPARE(QCoreApplication::translate("RepoView", conflicts, nullptr, 1),
           QString("1 file has conflicts. Keep one version or edit it, then "
                   "stage it to mark it resolved."));
  QCOMPARE(QCoreApplication::translate("RepoView", conflicts, nullptr, 2),
           QString("2 files have conflicts. For each one, keep one version or "
                   "edit it, then stage it to mark it resolved."));

  QCoreApplication::removeTranslator(&english);
}

void TestStateBanner::warnsAboutADetachedHead() {
  git::Reference head = mRepo.head();
  QVERIFY(head.isLocalBranch());
  mView->checkout(head, true);

  // Plain words, and it says what's at stake.
  QString id = mRepo.head().target().shortId();
  QTRY_VERIFY(banner()->isVisible());
  QCOMPARE(banner()->message(),
           QString("You're not on a branch. You're viewing commit %1. "
                   "Anything you commit here isn't kept on a branch and is "
                   "easy to lose.")
               .arg(id));

  QCOMPARE(label("BranchLabel")->text(),
           QString("Not on a branch (viewing %1)").arg(id));
  QVERIFY(label("UpstreamLabel")->isHidden());
}

void TestStateBanner::offersTheWayBack() {
  // Going back to where the user came from comes first.
  QCOMPARE(buttonTexts(),
           QStringList({"Back to " + mBranch, "Create Branch..."}));
}

void TestStateBanner::goesAwayOnABranchAgain() {
  button("Back to " + mBranch)->click();

  QTRY_VERIFY(!banner()->isVisible());
  QVERIFY(mRepo.head().isLocalBranch());
  QCOMPARE(mRepo.head().name(), mBranch);
}

void TestStateBanner::cleanupTestCase() { mWindow->close(); }

TEST_MAIN(TestStateBanner)

#include "state_banner.moc"
