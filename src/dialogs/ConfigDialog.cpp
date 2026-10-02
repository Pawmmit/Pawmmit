//
//          Copyright (c) 2016, Scientific Toolworks, Inc.
//
// This software is licensed under the GNU General Public License v3.0 or
// (at your option) any later version. The LICENSE.md file describes the
// conditions under which this software may be distributed.
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Author: Bryan Williams
//

#include "ConfigDialog.h"
#include "BranchesPanel.h"
#include "DiffPanel.h"
#include "LfsPanel.h"
#include "PluginsPanel.h"
#include "RemotesPanel.h"
#include "RepoGeneralPanel.h"
#include "SearchPanel.h"
#include "SubmodulesPanel.h"
#include "ui/blame/BlameEditor.h"
#include "ui/repo/RepoView.h"
#include "ui/window/EditorWindow.h"
#include <QAction>
#include <QActionGroup>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QShortcut>
#include <QStackedWidget>
#include <QToolBar>
#include <QVBoxLayout>

namespace {

class StackedWidget : public QStackedWidget {
public:
  StackedWidget(QWidget *parent = nullptr) : QStackedWidget(parent) {}

  QSize sizeHint() const override { return currentWidget()->sizeHint(); }

  QSize minimumSizeHint() const override {
    return currentWidget()->minimumSizeHint();
  }
};

} // namespace

ConfigDialog::ConfigDialog(RepoView *view, Index index) : QDialog(view) {
  setMinimumWidth(500);
  setAttribute(Qt::WA_DeleteOnClose);
  setContextMenuPolicy(Qt::NoContextMenu);

  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // Close on escape.
  QShortcut *esc = new QShortcut(tr("Esc"), this);
  connect(esc, &QShortcut::activated, this, &ConfigDialog::close);

  // Create tool bar.
  QToolBar *toolbar = new QToolBar(this);
  toolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  toolbar->setMovable(false);
  layout->addWidget(toolbar);

  // Create central stack widget.
  mStack = new StackedWidget(this);
  connect(mStack, &StackedWidget::currentChanged, this,
          &ConfigDialog::adjustSize);

  layout->addWidget(mStack);

  // Track actions in a group.
  mActions = new QActionGroup(this);
  connect(mActions, &QActionGroup::triggered, this, [this](QAction *action) {
    mStack->setCurrentIndex(mActions->actions().indexOf(action));
    setWindowTitle(action->text());
  });

  // Add project panel.
  QAction *general = toolbar->addAction(QIcon(":/general.png"), tr("General"));
  general->setActionGroup(mActions);
  general->setCheckable(true);

  RepoGeneralPanel *generalPanel = new RepoGeneralPanel(view, this);
  mStack->addWidget(generalPanel);

  // Add diff panel.
  QAction *diff = toolbar->addAction(QIcon(":/diff.png"), tr("Diff"));
  diff->setActionGroup(mActions);
  diff->setCheckable(true);

  DiffPanel *diffPanel = new DiffPanel(view->repo(), this);
  mStack->addWidget(diffPanel);

  // Add remotes panel.
  QAction *remotes = toolbar->addAction(QIcon(":/remotes.png"), tr("Remotes"));
  remotes->setActionGroup(mActions);
  remotes->setCheckable(true);

  mStack->addWidget(new RemotesPanel(view->repo(), this));

  // Add branches panel.
  QAction *branches =
      toolbar->addAction(QIcon(":/branches.png"), tr("Branches"));
  branches->setActionGroup(mActions);
  branches->setCheckable(true);

  mStack->addWidget(new BranchesPanel(view->repo(), this));

  // Add submodules panel.
  QAction *submodules =
      toolbar->addAction(QIcon(":/submodules.png"), tr("Submodules"));
  submodules->setActionGroup(mActions);
  submodules->setCheckable(true);

  mStack->addWidget(new SubmodulesPanel(view, this));

  // Add search panel.
  QAction *search = toolbar->addAction(QIcon(":/search.png"), tr("Search"));
  search->setActionGroup(mActions);
  search->setCheckable(true);

  mStack->addWidget(new SearchPanel(view, this));

  // Add plugins panel.
  QAction *plugins = toolbar->addAction(QIcon(":/plugins.png"), tr("Plugins"));
  plugins->setActionGroup(mActions);
  plugins->setCheckable(true);

  mStack->addWidget(new PluginsPanel(view->repo(), this));

  // Add LFS panel.
  QAction *lfs = toolbar->addAction(QIcon(":/lfs.png"), tr("LFS"));
  lfs->setActionGroup(mActions);
  lfs->setCheckable(true);

  mStack->addWidget(new LfsPanel(view, this));

  // Trigger the requested action.
  mActions->actions().at(index)->trigger();

  QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  // Add edit button.
  QPushButton *edit = buttons->addButton(tr("Edit Config File..."),
                                         QDialogButtonBox::ResetRole);
  connect(edit, &QPushButton::clicked, this, [view, generalPanel] {
    QString file = view->repo().dir().filePath("config");
    if (EditorWindow *window = view->openEditor(file))
      connect(window->widget(), &BlameEditor::saved, generalPanel,
              &RepoGeneralPanel::init);
  });

  // FIXME: Adding the button box directly to the layout
  // leaves weird margins at the sides of the dialog.
  QVBoxLayout *buttonLayout = new QVBoxLayout;
  buttonLayout->setContentsMargins(12, 0, 12, 12);
  buttonLayout->addWidget(buttons);
  layout->addLayout(buttonLayout);
}

void ConfigDialog::addRemote(const QString &name) {
  mActions->actions().at(Remotes)->trigger();
  static_cast<RemotesPanel *>(mStack->currentWidget())->addRemote(name);
}

void ConfigDialog::editBranch(const QString &name) {
  mActions->actions().at(Branches)->trigger();
  static_cast<BranchesPanel *>(mStack->currentWidget())->editBranch(name);
}

void ConfigDialog::showEvent(QShowEvent *event) {
  QDialog::showEvent(event);
  adjustSize();
}
