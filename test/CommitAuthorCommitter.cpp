#include "Test.h"

#include "ui/repo/RepoView.h"
#include "ui/window/MainWindow.h"

#include "conf/Settings.h"

#include "git/Reference.h"
#include "git/Signature.h"

#define INIT_REPO(repoPath)                                                    \
  QString path = Test::extractRepository(repoPath);                            \
  QVERIFY(!path.isEmpty());                                                    \
  git::Repository repo = git::Repository::open(path);                          \
  QVERIFY(repo.isValid());                                                     \
  Test::initRepo(repo);

using namespace QTest;

class TestCommitAuthorCommitter : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void testCherryPickAuthorEmailPreservance();
  void testRevertAuthorIsCurrentUser();
  void cleanupTestCase();

private:
  bool promptCherryPick;
  bool promptRevert;
};

void TestCommitAuthorCommitter::initTestCase() {
  promptCherryPick = Settings::instance()->prompt(Prompt::Kind::CherryPick);
  promptRevert = Settings::instance()->prompt(Prompt::Kind::Revert);
}

void TestCommitAuthorCommitter::cleanupTestCase() {
  Settings::instance()->setPrompt(Prompt::Kind::CherryPick, promptCherryPick);
  Settings::instance()->setPrompt(Prompt::Kind::Revert, promptRevert);
}

/*!
 * \brief TestCommitAuthorCommitter::testAuthorEmailPreservance
 * Check that author and email address are preserved during cherry pick
 */
void TestCommitAuthorCommitter::testCherryPickAuthorEmailPreservance() {
  INIT_REPO("CherryPickAuthorEmail.zip");

  git::Commit commit =
      repo.lookupCommit("710846db7a1fbd583975da0a6c10f9c2964ebd08");
  QVERIFY(commit.isValid());

  auto *window = new MainWindow(repo);
  RepoView *view = window->currentView();

  // Commit directly
  Settings::instance()->setPrompt(Prompt::Kind::CherryPick, false);
  view->cherryPick(commit);

  git::Reference master = repo.lookupRef(QString("refs/heads/master"));
  QVERIFY(master.isValid());
  auto c = master.annotatedCommit().commit();
  QCOMPARE(c.message(), "commit");
  QCOMPARE(c.author().email(), "test.author@test.com");
  QCOMPARE(c.author().name(), "TestAuthor");

  git::Signature signature = repo.defaultSignature();

  QCOMPARE(c.committer().email(), signature.email());
  QCOMPARE(c.committer().name(), signature.name());
}

/*!
 * \brief TestCommitAuthorCommitter::testRevertAuthorIsCurrentUser
 * Test that a revert is authored and committed by the current user, like git.
 */
void TestCommitAuthorCommitter::testRevertAuthorIsCurrentUser() {
  INIT_REPO("CherryPickAuthorEmail.zip");

  git::Commit commit =
      repo.lookupCommit("710846db7a1fbd583975da0a6c10f9c2964ebd08");
  QVERIFY(commit.isValid());

  auto *window = new MainWindow(repo);
  RepoView *view = window->currentView();

  git::Reference branch = repo.lookupRef(QString("refs/heads/secondBranch"));
  view->checkout(branch);

  QVERIFY(branch.isValid());
  auto c = branch.annotatedCommit().commit();
  QCOMPARE(c.message(), "commit");
  QCOMPARE(c.author().email(), "test.author@test.com");
  QCOMPARE(c.author().name(), "TestAuthor");

  // Commit directly
  Settings::instance()->setPrompt(Prompt::Kind::Revert, false);
  view->revert(commit);

  // This lookup is important, because otherwise it is not up to date
  branch = repo.lookupRef(QString("refs/heads/secondBranch"));
  QVERIFY(branch.isValid());
  c = branch.annotatedCommit().commit();
  QCOMPARE(c.message(), "Revert \"commit\"\n\nThis reverts commit "
                        "710846db7a1fbd583975da0a6c10f9c2964ebd08.");

  git::Signature signature = repo.defaultSignature();
  QCOMPARE(c.author().email(), signature.email());
  QCOMPARE(c.author().name(), signature.name());
  QCOMPARE(c.committer().email(), signature.email());
  QCOMPARE(c.committer().name(), signature.name());
}

TEST_MAIN(TestCommitAuthorCommitter)
#include "CommitAuthorCommitter.moc"
