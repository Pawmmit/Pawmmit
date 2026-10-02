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

#include "CheckoutDialog.h"
#include "git/Branch.h"
#include "ui/references/ReferenceList.h"
#include "ui_CheckoutDialog.h"
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QPushButton>

CheckoutDialog::CheckoutDialog(const git::Repository &repo,
                               const git::Reference &ref, QWidget *parent)
    : QDialog(parent), ui(new Ui::CheckoutDialog) {
  setAttribute(Qt::WA_DeleteOnClose);

  ui->setupUi(this);

  ui->mRefs->setRepository(repo, ReferenceView::AllRefs);
  connect(ui->mRefs, &ReferenceList::referenceSelected, this,
          &CheckoutDialog::update);

  connect(ui->mDetachBox, &QCheckBox::toggled, [this](bool checked) {
    if (ui->mDetachBox->isEnabled()) {
      mDetach = checked;
      update(ui->mRefs->currentReference());
    }
  });

  mCheckout =
      ui->mButtons->addButton(tr("Checkout"), QDialogButtonBox::AcceptRole);
  connect(ui->mButtons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(ui->mButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  ui->mRefs->select(ref);
  update(ui->mRefs->currentReference());
}

CheckoutDialog::~CheckoutDialog() = default;

git::Reference CheckoutDialog::reference() const {
  return ui->mRefs->currentReference();
}

void CheckoutDialog::update(const git::Reference &ref) {
  if (!ref.isValid()) {
    ui->mDetachBox->setEnabled(false);
    mCheckout->setEnabled(false);
    return;
  }

  bool local = ref.isLocalBranch();
  ui->mDetachBox->setEnabled(local);
  ui->mDetachBox->setChecked(!ui->mDetachBox->isEnabled() || mDetach);
  mCheckout->setEnabled(!ref.isHead() ||
                        (local && ui->mDetachBox->isChecked()));
}
