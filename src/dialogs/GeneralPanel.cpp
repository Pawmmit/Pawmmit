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

#include "GeneralPanel.h"
#include "AboutDialog.h"
#include "LanguageComboBox.h"
#include "conf/Settings.h"
#include "cred/CredentialHelper.h"
#include "git/Config.h"
#include "ui/repo/RepoView.h"
#include "ui/window/MainWindow.h"
#include "ui_GeneralPanel.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>

GeneralPanel::GeneralPanel(QWidget *parent)
    : QWidget(parent), ui(new Ui::GeneralPanel) {
  ui->setupUi(this);

  connect(ui->mFetch, &QCheckBox::toggled, ui->mFetchMinutes,
          &QSpinBox::setEnabled);

  setupLanguageComboBox(ui->mLanguages);

  connect(ui->mPrivacy, &QLabel::linkActivated,
          [] { AboutDialog::openSharedInstance(AboutDialog::Privacy); });

#if defined(Q_OS_MACOS)
  ui->formLayout->setRowVisible(ui->mSingleInstance, false);
#endif

  init();

  // Connect signals after initializing fields.
  connect(ui->mName, &QLineEdit::textChanged, [](const QString &text) {
    git::Config config = git::Config::global();
    config.setValue("user.name", text);
  });

  connect(ui->mEmail, &QLineEdit::textChanged, [](const QString &text) {
    git::Config config = git::Config::global();
    config.setValue("user.email", text);
  });

  connect(ui->mFetch, &QCheckBox::toggled, this, [](bool checked) {
    Settings::instance()->setValue(Setting::Id::FetchAutomatically, checked);
    for (MainWindow *window : MainWindow::windows()) {
      for (int i = 0; i < window->count(); ++i)
        window->view(i)->startFetchTimer();
    }
  });

  auto signal = QOverload<int>::of(&QSpinBox::valueChanged);
  connect(ui->mFetchMinutes, signal, [](int value) {
    Settings::instance()->setValue(Setting::Id::AutomaticFetchPeriodInMinutes,
                                   value);
  });

  connect(ui->mPushCommit, &QCheckBox::toggled, [](bool checked) {
    Settings::instance()->setValue(Setting::Id::PushAfterEachCommit, checked);
  });

  connect(ui->mPullUpdate, &QCheckBox::toggled, [](bool checked) {
    Settings::instance()->setValue(
        Setting::Id::UpdateSubmodulesAfterPullAndClone, checked);
  });

  connect(ui->mAutoPrune, &QCheckBox::toggled, [](bool checked) {
    Settings::instance()->setValue(Setting::Id::PruneAfterFetch, checked);
  });

  connect(ui->mAvailableStores,
          QOverload<int>::of(&QComboBox::currentIndexChanged),
          [this](int index) {
            git::Config config = git::Config::global();
            auto store = ui->mAvailableStores->itemData(index).toString();
            if (store.isEmpty()) {
              config.remove("credential.helper");
            } else {
              config.setValue("credential.helper", store);
            }

            delete CredentialHelper::instance();
          });

  connect(ui->mSingleInstance, &QCheckBox::toggled, [](bool checked) {
    Settings::instance()->setValue(Setting::Id::AllowSingleInstanceOnly,
                                   checked);
  });
}

GeneralPanel::~GeneralPanel() = default;

void GeneralPanel::init() {
  git::Config config = git::Config::global();
  ui->mName->setText(config.value<QString>("user.name"));
  ui->mEmail->setText(config.value<QString>("user.email"));

  Settings *settings = Settings::instance();

  ui->mFetch->setChecked(
      settings->value(Setting::Id::FetchAutomatically).toBool());
  ui->mFetchMinutes->setValue(
      settings->value(Setting::Id::AutomaticFetchPeriodInMinutes).toInt());

  ui->mPushCommit->setChecked(
      settings->value(Setting::Id::PushAfterEachCommit).toBool());
  ui->mPullUpdate->setChecked(
      settings->value(Setting::Id::UpdateSubmodulesAfterPullAndClone).toBool());
  ui->mAutoPrune->setChecked(
      settings->value(Setting::Id::PruneAfterFetch).toBool());

  auto currentHelper = config.value<QString>("credential.helper");

  ui->mAvailableStores->clear();
  ui->mAvailableStores->addItem(tr("None"), QString());
  for (const auto &helper : CredentialHelper::getAvailableHelperInformation()) {
    ui->mAvailableStores->addItem(helper.name, helper.name);
    ui->mAvailableStores->setItemData(ui->mAvailableStores->count() - 1,
                                      helper.description, Qt::ToolTipRole);
  }

  // Preserve an existing helper we don't otherwise recognize (e.g. a custom
  // script) instead of silently switching the selection to None.
  int index = ui->mAvailableStores->findData(currentHelper);
  if (index < 0 && !currentHelper.isEmpty()) {
    ui->mAvailableStores->addItem(currentHelper, currentHelper);
    index = ui->mAvailableStores->count() - 1;
  }

  ui->mAvailableStores->setCurrentIndex(index < 0 ? 0 : index);

  ui->mSingleInstance->setChecked(
      settings->value(Setting::Id::AllowSingleInstanceOnly).toBool());
}
