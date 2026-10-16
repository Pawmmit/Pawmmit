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

#include "MergeDialog.h"
#include "conf/Settings.h"
#include "git/Branch.h"
#include "ui/references/ReferenceList.h"
#include "ui_MergeDialog.h"
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>

namespace {

const ReferenceView::Kinds kRefKinds =
    ReferenceView::InvalidRef | ReferenceView::LocalBranches |
    ReferenceView::RemoteBranches | ReferenceView::Tags |
    ReferenceView::ExcludeHead;

} // namespace

MergeDialog::MergeDialog(RepoView::MergeFlags flags,
                         const git::Repository &repo, QWidget *parent)
    : QDialog(parent), mRepo(repo), ui(new Ui::MergeDialog) {
  setAttribute(Qt::WA_DeleteOnClose);

  ui->setupUi(this);

  ui->mRefs->setRepository(repo, kRefKinds);
  connect(ui->mRefs, &ReferenceList::referenceSelected, this,
          &MergeDialog::update);

  auto noff = RepoView::Merge | RepoView::NoFastForward;
  auto ffonly = RepoView::Merge | RepoView::FastForward;

  ui->mAction->addItem(tr("Merge"), RepoView::Merge);
  ui->mAction->addItem(tr("Rebase"), RepoView::Rebase);
  ui->mAction->addItem(tr("Squash"), RepoView::Squash);
  ui->mAction->addItem(tr("Merge (No Fast-forward)"), noff);
  ui->mAction->addItem(tr("Merge (Fast-forward Only)"), ffonly);
  ui->mAction->setCurrentIndex(ui->mAction->findData(static_cast<int>(flags)));

  ui->mLabel->setText(labelText());
  setWindowTitle(buttonText());

  ui->mNoCommit->setChecked(!Settings::instance()
                                 ->value(Setting::Id::CommitMergeImmediately)
                                 .toBool());
  connect(ui->mNoCommit, &QCheckBox::toggled, [](bool checked) {
    Settings::instance()->setValue(Setting::Id::CommitMergeImmediately,
                                   !checked);
  });

  ui->mNoCommit->setVisible(flags & RepoView::Merge);

  auto signal = QOverload<int>::of(&QComboBox::currentIndexChanged);
  connect(ui->mAction, signal, [this]() {
    RepoView::MergeFlags flags = this->flags();
    bool merge = (flags & RepoView::Merge);
    bool ffonly = (flags & RepoView::FastForward);

    ui->mLabel->setText(labelText());
    setWindowTitle(buttonText());
    mAccept->setText(buttonText());
    ui->mNoCommit->setVisible(merge && !ffonly);
  });

  mAccept = ui->mButtons->addButton(buttonText(), QDialogButtonBox::AcceptRole);
  connect(ui->mButtons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(ui->mButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  update();
}

MergeDialog::~MergeDialog() = default;

git::Commit MergeDialog::target() const { return ui->mRefs->target(); }

git::Reference MergeDialog::reference() const {
  return ui->mRefs->currentReference();
}

RepoView::MergeFlags MergeDialog::flags() const {
  int action = ui->mAction->itemData(ui->mAction->currentIndex()).toInt();
  RepoView::MergeFlags flags = static_cast<RepoView::MergeFlags>(action);
  if (!Settings::instance()
           ->value(Setting::Id::CommitMergeImmediately)
           .toBool())
    flags |= RepoView::NoCommit;
  return flags;
}

void MergeDialog::setCommit(const git::Commit &commit) {
  ui->mRefs->setCommit(commit);
  update();
}

void MergeDialog::setReference(const git::Reference &ref) {
  ui->mRefs->select(ref);
  update();
}

void MergeDialog::update() {
  mAccept->setEnabled(ui->mRefs->target().isValid());
}

QString MergeDialog::labelText() const {
  QString fmt;
  if (flags() & RepoView::Merge)
    fmt = tr("Choose a reference to merge into '%1'.");
  else if (flags() & RepoView::Rebase)
    fmt = tr("Choose a reference to rebase '%1' on.");
  else
    fmt = tr("Choose a reference to squash into '%1'.");

  git::Reference head = mRepo.head();
  Q_ASSERT(head.isValid());

  return fmt.arg(head.name(false));
}

QString MergeDialog::buttonText() const {
  if (flags() & RepoView::Merge)
    return tr("Merge");

  return (flags() & RepoView::Rebase) ? tr("Rebase") : tr("Squash");
}
