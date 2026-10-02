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

#include "RemoteDialog.h"
#include "conf/Settings.h"
#include "git/Config.h"
#include "git/Remote.h"
#include "git/Repository.h"
#include "ui/ExpandButton.h"
#include "ui/references/ReferenceList.h"
#include "ui/repo/RepoView.h"
#include "ui_RemoteDialog.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>

RemoteDialog::RemoteDialog(Kind kind, RepoView *parent)
    : QDialog(parent), ui(new Ui::RemoteDialog) {
  git::Repository repo = parent->repo();
  setAttribute(Qt::WA_DeleteOnClose);

  ui->setupUi(this);

  for (const git::Remote &remote : repo.remotes())
    ui->mRemotes->addItem(remote.name(), QVariant::fromValue(remote));

  git::Remote defaultRemote = repo.defaultRemote();
  if (defaultRemote.isValid()) {
    int index = ui->mRemotes->findText(defaultRemote.name());
    if (index >= 0)
      ui->mRemotes->setCurrentIndex(index);
  }

  QString tagsText =
      (kind == Push) ? tr("Push all tags") : tr("Update existing tags");
  ui->mTags->setText(tagsText);

  ui->formLayout->setRowVisible(ui->mAction, kind == Pull);
  if (kind == Pull) {
    auto noff = RepoView::Merge | RepoView::NoFastForward;
    auto ffonly = RepoView::Merge | RepoView::FastForward;

    ui->mAction->addItem(tr("Merge"), RepoView::Merge);
    ui->mAction->addItem(tr("Rebase"), RepoView::Rebase);
    ui->mAction->addItem(tr("Merge (No Fast-forward)"), noff);
    ui->mAction->addItem(tr("Merge (Fast-forward Only)"), ffonly);
  }

  ui->formLayout->setRowVisible(ui->mPrune, kind != Push);
  if (kind != Push) {
    bool autoPrune =
        Settings::instance()->value(Setting::Id::PruneAfterFetch).toBool();
    git::Config config = repo.appConfig();

    ui->mPrune->setChecked(config.value<bool>("autoprune.enable", autoPrune));
  }

  ui->formLayout->setRowVisible(ui->mRefs, kind == Push);
  ui->formLayout->setRowVisible(ui->mSetUpstream, kind == Push);
  ui->formLayout->setRowVisible(ui->mForce, kind == Push);
  ui->formLayout->setRowVisible(ui->mRemoteRef, kind == Push);
  if (kind == Push) {
    auto kinds = ReferenceView::LocalBranches | ReferenceView::Tags;
    ui->mRefs->setRepository(repo, kinds);

    connect(ui->mRefs, &ReferenceList::referenceSelected,
            [this](const git::Reference &ref) {
              QString value = QString();
              if (ref.isValid()) {
                QString key = QString("branch.%1.merge").arg(ref.name());
                git::Config config =
                    RepoView::parentView(this)->repo().gitConfig();
                value = config.value<QString>(key);
              }
              ui->mRemoteRef->setText(value);
            });

    ui->mRefs->select(repo.head());
  }

  QString button;
  switch (kind) {
    case Fetch:
      button = tr("Fetch");
      break;

    case Pull:
      button = tr("Pull");
      break;

    case Push:
      button = tr("Push");
      break;
  }
  setWindowTitle(button);

  connect(ui->mButtons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(ui->mButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  ui->mButtons->addButton(button, QDialogButtonBox::AcceptRole);

  connect(this, &RemoteDialog::accepted, [this, kind] {
    RepoView *view = RepoView::parentView(this);
    QString remoteName = ui->mRemotes->currentText();
    git::Remote tmp = ui->mRemotes->currentData().value<git::Remote>();
    git::Remote remote = (tmp.isValid() && tmp.name() == remoteName)
                             ? tmp
                             : view->repo().anonymousRemote(remoteName);
    bool tags = ui->mTags->isChecked();

    switch (kind) {
      case Fetch:
        view->fetch(remote, tags, true, nullptr, nullptr,
                    ui->mPrune->isChecked());
        break;

      case Pull: {
        RepoView::MergeFlags flags(ui->mAction->currentData().toInt());
        view->pull(flags, remote, tags, ui->mPrune->isChecked());
        break;
      }

      case Push: {
        git::Reference ref = ui->mRefs->currentReference();
        QString remoteRef = ui->mRemoteRef->text();
        bool setUpstream = ui->mSetUpstream->isChecked();
        bool force = ui->mForce->isChecked();
        view->push(remote, ref, remoteRef, setUpstream, force, tags);
        break;
      }
    }
  });
}

RemoteDialog::~RemoteDialog() = default;
