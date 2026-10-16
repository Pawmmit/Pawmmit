//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#ifndef REPOLINKHANDLER_H
#define REPOLINKHANDLER_H

#include <QString>

class RepoView;

namespace RepoLinkHandler {

// Dispatches a link activated in the view to the matching view action.
void visit(RepoView *view, const QString &link);

} // namespace RepoLinkHandler

#endif
