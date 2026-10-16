// This software is licensed under the GNU General Public License v3.0 or
// (at your option) any later version. The LICENSE.md file describes the
// conditions under which this software may be distributed.
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Author: Michael WERLE
//

#include "RenameBranchDialog.h"
#include "git/Branch.h"
#include "ui/ExpandButton.h"
#include "ui/references/ReferenceList.h"
#include "ui/repo/RepoView.h"
#include "ui_RenameBranchDialog.h"
#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>

RenameBranchDialog::RenameBranchDialog(const git::Repository &repo,
                                       const git::Branch &branch,
                                       QWidget *parent)
    : QDialog(parent), ui(new Ui::RenameBranchDialog) {
  Q_ASSERT(branch.isValid() && branch.isLocalBranch());
  setAttribute(Qt::WA_DeleteOnClose);

  ui->setupUi(this);
  ui->mName->setText(branch.name());

  connect(ui->mButtons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(ui->mButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  // Custom-role, custom-text button: Designer's QDialogButtonBox only
  // supports standard buttons declaratively.
  QPushButton *rename = ui->mButtons->addButton(tr("Rename Branch"),
                                                QDialogButtonBox::AcceptRole);
  rename->setEnabled(false);

  // Update button when name text changes.
  connect(ui->mName, &QLineEdit::textChanged,
          [repo, rename](const QString &text) {
            rename->setEnabled(
                git::Branch::isNameValid(text) &&
                !repo.lookupBranch(text, GIT_BRANCH_LOCAL).isValid());
          });

  // Perform the rename when the button is clicked
  connect(rename, &QPushButton::clicked,
          [this, branch] { git::Branch(branch).rename(ui->mName->text()); });
}

RenameBranchDialog::~RenameBranchDialog() = default;

QString RenameBranchDialog::name() const { return ui->mName->text(); }
