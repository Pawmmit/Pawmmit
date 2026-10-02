//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#ifndef COMMITCONTEXTMENU_H
#define COMMITCONTEXTMENU_H

#include "git/Reference.h"
#include <QMenu>
#include <QModelIndex>

class CommitList;

// Context menu for a row of the commit list, using its selection.
class CommitContextMenu : public QMenu {
public:
  /// @param list Commit list the menu is shown for
  /// @param index Row that was clicked
  /// @param listRef Reference the list is showing
  CommitContextMenu(CommitList *list, const QModelIndex &index,
                    const git::Reference &listRef);
};

#endif
