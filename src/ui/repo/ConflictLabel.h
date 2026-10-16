//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#ifndef CONFLICTLABEL_H
#define CONFLICTLABEL_H

#include "git/Repository.h"
#include <tuple>
#include <QString>

namespace conflict {
/// @brief Determine newbie friendly version of 'ours' and 'theirs'
/// @param repo Repository to work with
/// @return Returned in the form 'Ours, Theirs'
std::tuple<QString, QString> labels(const git::Repository &repo);

/// @brief The incoming branch or commit, e.g. "fix/foobar" or "commit a3c9dc".
/// @param repo Repository to work with
/// @return Incoming branch name or commit
QString incomingName(const git::Repository &repo);

} // namespace conflict

#endif
