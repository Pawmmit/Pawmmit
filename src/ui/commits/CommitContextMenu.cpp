//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "CommitContextMenu.h"
#include "CommitList.h"
#include "dialogs/DiffFileDialog.h"
#include "dialogs/MergeDialog.h"
#include "git/AnnotatedCommit.h"
#include "git/Commit.h"
#include "git/Diff.h"
#include "log/LogEntry.h"
#include "ui/repo/RepoView.h"
#include <QFileInfo>
#include <QItemSelectionModel>
#include <QSaveFile>
#include <functional>

namespace {

/// @brief Helper function to add a list of items to a menu.
/// A single item is added directly to the menu, whereas multiple items will
/// be added to a sub-menu.
void addMenuEntries(QMenu &menu, const QString &operation,
                    const QList<git::Reference> &items,
                    std::function<void(const git::Reference &)> action) {
  QMenu *submenu = &menu;
  QString entryName(operation + " %1");
  if (items.count() > 1) {
    submenu = menu.addMenu(operation);
    entryName = QString("%1");
  }
  for (const git::Reference &ref : items) {
    submenu->addAction(entryName.arg(ref.name()),
                       [action, ref] { action(ref); });
  }
}

void saveDiff(CommitList *list, const QString &path) {
  RepoView *view = RepoView::parentView(list);
  LogEntry *entry = view->addLogEntry(path, CommitList::tr("Save Diff As"));

  QSaveFile file(path);
  if (file.open(QSaveFile::WriteOnly)) {
    QByteArray buffer = list->selectedDiff().toBuffer();
    if (file.write(buffer) != -1)
      file.commit();
  }

  if (file.error() != QSaveFile::NoError)
    view->error(entry, CommitList::tr("save diff"), QFileInfo(path).fileName(),
                file.errorString());
}

} // namespace

CommitContextMenu::CommitContextMenu(CommitList *list, const QModelIndex &index,
                                     const git::Reference &listRef) {
  RepoView *view = RepoView::parentView(list);
  git::Commit commit = index.data(CommitList::CommitRole).value<git::Commit>();

  if (!commit.isValid()) {
    // clean
    QStringList untracked;
    if (git::Diff diff = list->status()) {
      for (int i = 0; i < diff.count(); i++) {
        if (diff.status(i) == GIT_DELTA_UNTRACKED)
          untracked.append(diff.name(i));
      }
    }

    QAction *clean = addAction(CommitList::tr("Remove Untracked Files"),
                               [view, untracked] { view->clean(untracked); });

    clean->setEnabled(!untracked.isEmpty());

    return;
  }

  setToolTipsVisible(true);

  // stash
  if (listRef.isValid() && listRef.isStash()) {
    addAction(CommitList::tr("Apply"),
              [view, index] { view->applyStash(index.row()); });

    addAction(CommitList::tr("Pop"),
              [view, index] { view->popStash(index.row()); });

    addAction(CommitList::tr("Drop"),
              [view, index] { view->dropStash(index.row()); });

  } else {
    // multiple selection
    bool anyStarred = false, allValid = true;
    for (const QModelIndex &index : list->selectionModel()->selectedIndexes()) {
      QVariant variant = index.data(CommitList::CommitRole);
      if (!variant.isValid()) {
        allValid = false;
        continue;
      } else if (variant.value<git::Commit>().isStarred()) {
        anyStarred = true;
      }
    }

    addAction(anyStarred ? CommitList::tr("Unstar") : CommitList::tr("Star"),
              [list, anyStarred] {
                for (const QModelIndex &index :
                     list->selectionModel()->selectedIndexes())
                  if (index.data(CommitList::CommitRole).isValid())
                    index.data(CommitList::CommitRole)
                        .value<git::Commit>()
                        .setStarred(!anyStarred);
              });

    QAction *saveDiffAs = addAction(CommitList::tr("Save Diff As..."), [list] {
      QString path = DiffFileDialog::getSaveFileName(list);
      if (!path.isEmpty())
        saveDiff(list, path);
    });

    saveDiffAs->setEnabled(allValid);

    // single selection
    if (list->selectionModel()->selectedIndexes().size() <= 1) {
      addSeparator();

      addAction(CommitList::tr("Add Tag..."),
                [view, commit] { view->promptToAddTag(commit); });

      addAction(CommitList::tr("New Branch..."),
                [view, commit] { view->promptToCreateBranch(commit); });

      // Add operations on existing references; there may be 0, 1, or multiple
      // of each type of reference on a commit.
      QList<git::Reference> rename_branches;
      QList<git::Reference> tags;
      QList<git::Reference> delete_branches;
      QList<git::Reference> all_branches; // used later
      for (const git::Reference &ref : commit.refs()) {
        if (ref.isTag()) {
          tags.append(ref);
        } else if (ref.isBranch()) {
          all_branches.append(ref);
          if (ref.isLocalBranch()) {
            rename_branches.append(ref);
            if (view->repo().head().name() != ref.name()) {
              delete_branches.append(ref);
            }
          }
        }
      }

      if (rename_branches.count() > 0 || delete_branches.count() > 0 ||
          tags.count() > 0) {
        addSeparator();
      }
      addMenuEntries(*this, CommitList::tr("Rename Branch"), rename_branches,
                     std::bind(&RepoView::promptToRenameBranch, view,
                               std::placeholders::_1));

      addMenuEntries(*this, CommitList::tr("Delete Branch"), delete_branches,
                     std::bind(&RepoView::promptToDeleteBranch, view,
                               std::placeholders::_1));

      addMenuEntries(
          *this, CommitList::tr("Delete Tag"), tags,
          std::bind(&RepoView::promptToDeleteTag, view, std::placeholders::_1));
      addSeparator();

      addAction(CommitList::tr("Merge..."), [view, commit] {
        MergeDialog *dialog =
            new MergeDialog(RepoView::Merge, view->repo(), view);
        connect(dialog, &QDialog::accepted, [view, dialog] {
          git::AnnotatedCommit upstream;
          git::Reference ref = dialog->reference();
          if (!ref.isValid())
            upstream = dialog->target().annotatedCommit();
          view->merge(dialog->flags(), ref, upstream);
        });

        dialog->setCommit(commit);
        dialog->open();
      });

      addAction(CommitList::tr("Rebase..."), [view, commit] {
        MergeDialog *dialog =
            new MergeDialog(RepoView::Rebase, view->repo(), view);
        connect(dialog, &QDialog::accepted, [view, dialog] {
          git::AnnotatedCommit upstream;
          git::Reference ref = dialog->reference();
          if (!ref.isValid())
            upstream = dialog->target().annotatedCommit();
          view->merge(dialog->flags(), ref, upstream);
        });

        dialog->setCommit(commit);
        dialog->open();
      });

      addAction(CommitList::tr("Squash..."), [view, commit] {
        MergeDialog *dialog =
            new MergeDialog(RepoView::Squash, view->repo(), view);
        connect(dialog, &QDialog::accepted, [view, dialog] {
          git::AnnotatedCommit upstream;
          git::Reference ref = dialog->reference();
          if (!ref.isValid())
            upstream = dialog->target().annotatedCommit();
          view->merge(dialog->flags(), ref, upstream);
        });

        dialog->setCommit(commit);
        dialog->open();
      });

      addSeparator();

      addAction(CommitList::tr("Revert"),
                [view, commit] { view->revert(commit); });

      addAction(CommitList::tr("Cherry-pick"),
                [view, commit] { view->cherryPick(commit); });

      addSeparator();

      git::Reference head = view->repo().head();
      QMenu *submenu = this;
      auto entryName = CommitList::tr("Checkout %1");
      if (all_branches.count() > 1) {
        submenu = addMenu(CommitList::tr("Checkout"));
        entryName = QString("%1");
      }
      for (const git::Reference &ref : all_branches) {
        if (ref.isLocalBranch()) {
          QAction *checkout = submenu->addAction(
              entryName.arg(ref.name()), [view, ref] { view->checkout(ref); });

          checkout->setEnabled(head.isValid() &&
                               head.qualifiedName() != ref.qualifiedName() &&
                               !view->repo().isBare());
        } else if (ref.isRemoteBranch()) {
          QAction *checkout = submenu->addAction(
              entryName.arg(ref.name()), [view, ref] { view->checkout(ref); });

          // Calculate local branch name in the same way as checkout() does
          QString local = ref.name().section('/', 1);
          if (!head.isValid()) { // I'm not sure when this can happen
            checkout->setEnabled(false);
          } else if (head.name() == local) {
            checkout->setEnabled(false);
            checkout->setToolTip(
                CommitList::tr("Local branch is already checked out"));
          } else if (view->repo().isBare()) {
            checkout->setEnabled(false);
            checkout->setToolTip(CommitList::tr("This is a bare repository"));
          }
        }
      }

      QString name = commit.detachedHeadName();
      QAction *checkout = addAction(CommitList::tr("Checkout %1").arg(name),
                                    [view, commit] { view->checkout(commit); });

      checkout->setEnabled(head.isValid() && head.target() != commit &&
                           !view->repo().isBare());

      addSeparator();

      QMenu *reset = addMenu(CommitList::tr("Reset"));
      reset->addAction(CommitList::tr("Soft"))->setData(GIT_RESET_SOFT);
      reset->addAction(CommitList::tr("Mixed"))->setData(GIT_RESET_MIXED);
      reset->addAction(CommitList::tr("Hard"))->setData(GIT_RESET_HARD);
      connect(reset, &QMenu::triggered, [view, commit](QAction *action) {
        git_reset_t type = static_cast<git_reset_t>(action->data().toInt());
        view->promptToReset(commit, type);
      });

      reset->setEnabled(head.isValid() && head.isLocalBranch());
    }
  }
}
