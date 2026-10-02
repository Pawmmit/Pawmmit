//
//          Copyright (c) 2016, Scientific Toolworks, Inc.
//
// This software is licensed under the GNU General Public License v3.0 or
// (at your option) any later version. The LICENSE.md file describes the
// conditions under which this software may be distributed.
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Author: Shane Gramlich
//

#include "PullRequestDialog.h"
#include "host/Account.h"
#include "ui/repo/RepoView.h"
#include "ui_PullRequestDialog.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>

PullRequestDialog::PullRequestDialog(RepoView *view)
    : QDialog(view), ui(new Ui::PullRequestDialog) {
  setAttribute(Qt::WA_DeleteOnClose);

  ui->setupUi(this);

  setCommit(view->repo().head().target());

  for (const git::Reference &ref : view->repo().branches(GIT_BRANCH_LOCAL))
    ui->mFromRepo->addItem(ref.name(), QVariant::fromValue(ref));
  ui->mFromRepo->setCurrentIndex(
      ui->mFromRepo->findText(view->repo().head().name()));
  auto indexChanged = QOverload<int>::of(&QComboBox::currentIndexChanged);
  connect(ui->mFromRepo, indexChanged, this, [this](int index) {
    setCommit(ui->mFromRepo->itemData(index).value<git::Reference>().target());
  });

  Repository *remoteRepo = view->remoteRepo();

  ui->mToRepo->lineEdit()->setPlaceholderText(tr("owner/repository"));
  ui->mToBranch->lineEdit()->setPlaceholderText(tr("branch"));

  remoteRepo->account()->requestForkParents(remoteRepo);
  connect(remoteRepo->account(), &Account::forkParentsReady, this,
          [this](const QMap<QString, QString> &parents) {
            for (const QString &parent : parents.keys())
              ui->mToRepo->addItem(parent, parents.value(parent));
          });
  connect(ui->mToRepo, indexChanged, this, [this](int index) {
    ui->mToBranch->clear();
    ui->mToBranch->addItem(ui->mToRepo->itemData(index).toString());
  });

  QPushButton *create =
      ui->mButtons->addButton(tr("Create"), QDialogButtonBox::AcceptRole);
  create->setDefault(true);
  create->setEnabled(false);

  auto signal = QOverload<const QString &>::of(&QComboBox::currentTextChanged);
  connect(ui->mToRepo, signal, [this, create]() {
    bool valid = !ui->mToRepo->currentText().isEmpty() &&
                 !ui->mToBranch->currentText().isEmpty();
    create->setEnabled(valid);
  });
  connect(ui->mToBranch, signal, [this, create]() {
    bool valid = !ui->mToRepo->currentText().isEmpty() &&
                 !ui->mToBranch->currentText().isEmpty();
    create->setEnabled(valid);
  });

  connect(ui->mButtons, &QDialogButtonBox::accepted, this, &QDialog::close);
  connect(ui->mButtons, &QDialogButtonBox::rejected, this, &QDialog::close);
  connect(create, &QPushButton::clicked, [this, remoteRepo] {
    remoteRepo->account()->createPullRequest(
        remoteRepo, ui->mToRepo->currentText(), ui->mTitle->text(),
        ui->mBody->toPlainText(), ui->mFromRepo->currentText(),
        ui->mToBranch->currentText(), ui->mMaintainerEdit->isChecked());
  });
}

PullRequestDialog::~PullRequestDialog() = default;

void PullRequestDialog::setCommit(const git::Commit &commit) {
  ui->mTitle->setText(commit.summary());

  QString body;
  if (!commit.body().isEmpty())
    body = QString("%1\n\n").arg(commit.body());

  ui->mBody->setText(body);
}
