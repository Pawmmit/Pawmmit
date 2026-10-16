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

#include "NewBranchDialog.h"
#include "git/Reference.h"
#include "ui/ExpandButton.h"
#include "ui/references/ReferenceList.h"
#include "ui/repo/RepoView.h"
#include "ui_NewBranchDialog.h"
#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>

NewBranchDialog::NewBranchDialog(const git::Repository &repo,
                                 const git::Commit &commit, QWidget *parent)
    : QDialog(parent), ui(new Ui::NewBranchDialog) {
  setAttribute(Qt::WA_DeleteOnClose);

  ui->setupUi(this);

  auto kinds = ReferenceView::InvalidRef | ReferenceView::RemoteBranches;
  ui->mUpstream->setRepository(repo, kinds);

  kinds = ReferenceView::AllRefs;
  if (commit.isValid())
    kinds |= ReferenceView::InvalidRef;
  ui->mRefs->setRepository(repo, kinds);
  ui->mRefs->select(repo.head());
  ui->mRefs->setCommit(commit);

  // Only show the start-point row when there's no fixed commit target.
  ui->formLayout->setRowVisible(ui->mRefs, !commit.isValid());

  ui->mCheckout->setVisible(qobject_cast<RepoView *>(parent));

  QPushButton *create = ui->mButtons->addButton(tr("Create Branch"),
                                                QDialogButtonBox::AcceptRole);
  create->setEnabled(false);
  connect(ui->mButtons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(ui->mButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  // Update button when name text changes.
  connect(ui->mName, &QLineEdit::textChanged,
          [repo, create](const QString &text) {
            create->setEnabled(
                git::Branch::isNameValid(text) &&
                !repo.lookupBranch(text, GIT_BRANCH_LOCAL).isValid());
          });

  // Populate name and start point when upstream changes.
  connect(ui->mUpstream, &ReferenceList::referenceSelected,
          [this](const git::Reference &ref) {
            if (ref.isValid()) {
              if (ui->mName->text().isEmpty())
                ui->mName->setText(ref.name().section('/', -1));
              ui->mRefs->select(ref);
            }
          });
}

NewBranchDialog::~NewBranchDialog() = default;

QString NewBranchDialog::name() const { return ui->mName->text(); }

bool NewBranchDialog::checkout() const { return ui->mCheckout->isChecked(); }

git::Commit NewBranchDialog::target() const { return ui->mRefs->target(); }

git::Reference NewBranchDialog::upstream() const {
  return ui->mUpstream->currentReference();
}
