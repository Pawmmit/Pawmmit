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

#ifndef GIT_ID_H
#define GIT_ID_H

#include "git2/oid.h"
#include <QMetaType>
#include <QString>

namespace git {

class Id {
public:
  Id();
  Id(const QByteArray &id, git_oid_t type);
  Id(const git_oid &id);
  Id(const git_oid *id);

  bool isNull() const;
  bool isValid() const;

  QString toString() const;
  QByteArray toByteArray() const;

  bool operator<(const Id &rhs) const;
  bool operator==(const Id &rhs) const;
  bool operator!=(const Id &rhs) const;

  uint8_t getSize() const;
  static uint8_t getSize(const git_oid_t type);

private:
  operator const git_oid *() const;

  git_oid d;

  friend class Commit;
  friend class Index;
  friend class Repository;
  friend class RevWalk;
};

uint qHash(const Id &key);

} // namespace git

Q_DECLARE_METATYPE(git::Id);

#endif
