//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "ConflictLabel.h"
#include "git/Branch.h"
#include "git/Rebase.h"
#include "git/Reference.h"
#include <QCoreApplication>

// Git only remembers the incoming commit, so name it after a branch still
// pointing at it, preferring a local one.
static QString branchNameForCommit(const git::Repository &repo,
                                   const git::Commit &commit) {
  QString remoteMatch;
  for (const git::Branch &branch : repo.branches()) {
    git::Commit tip = branch.target();
    if (!tip.isValid() || tip.id() != commit.id())
      continue;

    if (branch.isLocalBranch())
      return branch.name();
    if (remoteMatch.isEmpty())
      remoteMatch = branch.name();
  }

  return remoteMatch;
}

static QString conflictTheirsName(const git::Repository &repo,
                                  const git::Rebase &rebase) {
  if (rebase.isValid()) {
    QString orig = rebase.origHeadName();
    if (!orig.isEmpty())
      return orig;
  }

  return conflict::incomingName(repo);
}

namespace conflict {
std::tuple<QString, QString> labels(const git::Repository &repo) {
  git::Reference head = repo.head();
  git::Rebase rebase = repo.rebaseOpen();

  QString ours;
  if (head.isLocalBranch())
    ours = head.name();
  else
    ours = rebase.isValid() ? rebase.ontoName() : QString();
  ours = !ours.isEmpty()
             ? QCoreApplication::translate("ConflictLabel", "Keep %1").arg(ours)
             : QCoreApplication::translate("ConflictLabel",
                                           "Keep current version");

  QString theirs;
  // A revert's incoming side is the file without the reverted commit's change.
  git::Commit reverted = repo.operation() == git::Operation::Revert
                             ? repo.incomingCommit()
                             : git::Commit();
  if (reverted.isValid()) {
    theirs = QCoreApplication::translate("ConflictLabel", "Undo commit %1")
                 .arg(reverted.shortId());
  } else {
    theirs = conflictTheirsName(repo, rebase);
    if (!theirs.isEmpty()) {
      theirs =
          QCoreApplication::translate("ConflictLabel", "Take %1").arg(theirs);
    } else {
      // Applying a stash is what leaves conflicts without an operation running.
      theirs = (repo.operation() == git::Operation::None)
                   ? QCoreApplication::translate("ConflictLabel",
                                                 "Take stashed version")
                   : QCoreApplication::translate("ConflictLabel",
                                                 "Take incoming version");
    }
  }
  return std::make_tuple(ours, theirs);
}

QString incomingName(const git::Repository &repo) {
  git::Commit commit = repo.incomingCommit();
  if (!commit.isValid())
    return QString();

  QString branch = branchNameForCommit(repo, commit);
  return !branch.isEmpty()
             ? branch
             : QCoreApplication::translate("ConflictLabel", "commit %1")
                   .arg(commit.shortId());
}
} // namespace conflict
