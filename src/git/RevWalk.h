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

#ifndef REVWALK_H
#define REVWALK_H

#include <QString>
#include <memory>

struct git_revwalk;

namespace git {

class Commit;
class Reference;

class RevWalk {
public:
  RevWalk();

  bool isValid() const { return d != nullptr; }

  bool hide(const Commit &commit);
  bool hide(const Reference &ref);

  bool push(const Commit &commit);
  bool push(const Reference &ref);

  // Only return commits that touch the given path; this can't be undone.
  bool setPathspec(const QString &pathspec);

  Commit next() const;

  // True once next() has returned every commit, rather than stopping early on
  // an error.
  bool isFinished() const { return mFinished; }

protected:
  RevWalk(git_revwalk *walker);

  // Null for an invalid walk, which libgit2 rejects with an error.
  git_revwalk *walk() const;

  struct Data;
  std::shared_ptr<Data> d;
  mutable bool mFinished = false;

  friend class Commit;
  friend class Reference;
  friend class Repository;
};

} // namespace git

#endif
