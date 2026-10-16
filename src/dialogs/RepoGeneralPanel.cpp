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

#include "RepoGeneralPanel.h"
#include "conf/Settings.h"
#include "git/Config.h"
#include "ui/repo/RepoView.h"
#include "ui_RepoGeneralPanel.h"
#include <QCheckBox>
#include <QLineEdit>
#include <QSpinBox>

RepoGeneralPanel::RepoGeneralPanel(RepoView *view, QWidget *parent)
    : QWidget(parent), mRepo(view->repo()), ui(new Ui::RepoGeneralPanel) {
  ui->setupUi(this);

  init();

  // Connect signals after initializing fields.
  connect(ui->mName, &QLineEdit::textChanged, this,
          [this](const QString &text) {
            git::Config config = mRepo.gitConfig();
            config.setValue("user.name", text);
          });

  connect(ui->mEmail, &QLineEdit::textChanged, this,
          [this](const QString &text) {
            git::Config config = mRepo.gitConfig();
            config.setValue("user.email", text);
          });

  connect(ui->mFetch, &QCheckBox::toggled, view, [this, view](bool checked) {
    git::Config config = mRepo.appConfig();
    config.setValue("autofetch.enable", checked);
    view->startFetchTimer();
  });

  using Signal = void (QSpinBox::*)(int);
  auto signal = static_cast<Signal>(&QSpinBox::valueChanged);
  connect(ui->mFetchMinutes, signal, this, [this](int value) {
    git::Config config = mRepo.appConfig();
    config.setValue("autofetch.minutes", value);
  });

  connect(ui->mPushCommit, &QCheckBox::toggled, this, [this](bool checked) {
    git::Config config = mRepo.appConfig();
    config.setValue("autopush.enable", checked);
  });

  connect(ui->mPullUpdate, &QCheckBox::toggled, this, [this](bool checked) {
    git::Config config = mRepo.appConfig();
    config.setValue("autoupdate.enable", checked);
  });

  connect(ui->mAutoPrune, &QCheckBox::toggled, this, [this](bool checked) {
    git::Config config = mRepo.appConfig();
    config.setValue("autoprune.enable", checked);
  });
}

RepoGeneralPanel::~RepoGeneralPanel() = default;

void RepoGeneralPanel::init() {
  git::Config config = mRepo.gitConfig();
  ui->mName->setText(config.value<QString>("user.name"));
  ui->mEmail->setText(config.value<QString>("user.email"));

  // Read defaults from global settings.
  Settings *settings = Settings::instance();
  bool fetch = settings->value(Setting::Id::FetchAutomatically).toBool();
  int minutes =
      settings->value(Setting::Id::AutomaticFetchPeriodInMinutes).toInt();

  bool push = settings->value(Setting::Id::PushAfterEachCommit).toBool();
  bool update =
      settings->value(Setting::Id::UpdateSubmodulesAfterPullAndClone).toBool();
  bool prune = settings->value(Setting::Id::PruneAfterFetch).toBool();

  git::Config app = mRepo.appConfig();
  ui->mFetch->setChecked(app.value<bool>("autofetch.enable", fetch));
  ui->mFetchMinutes->setValue(app.value<int>("autofetch.minutes", minutes));
  ui->mFetchMinutes->setEnabled(ui->mFetch->isChecked());
  ui->mPushCommit->setChecked(app.value<bool>("autopush.enable", push));
  ui->mPullUpdate->setChecked(app.value<bool>("autoupdate.enable", update));
  ui->mAutoPrune->setChecked(app.value<bool>("autoprune.enable", prune));
}
