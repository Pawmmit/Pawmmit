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

#include "SearchPanel.h"
#include "git/Config.h"
#include "index/Index.h"
#include "index/IndexerProcess.h"
#include "ui/repo/RepoView.h"
#include "ui_SearchPanel.h"
#include <QCheckBox>
#include <QSpinBox>

SearchPanel::SearchPanel(RepoView *view, QWidget *parent)
    : QWidget(parent), ui(new Ui::SearchPanel) {
  ui->setupUi(this);

  using Signal = void (QSpinBox::*)(int);
  auto signal = static_cast<Signal>(&QSpinBox::valueChanged);

  git::Config config = view->repo().appConfig();
  Q_ASSERT(config.isValid());

  ui->mEnable->setChecked(config.value<bool>("index.enable", true));
  connect(ui->mEnable, &QCheckBox::toggled, this, [view](bool checked) {
    git::Config config = view->repo().appConfig();
    config.setValue("index.enable", checked);

    if (checked) {
      view->indexer()->start();
    } else {
      view->indexer()->cancel();
    }
  });

  ui->mTerms->setValue(config.value<int>("index.termlimit", 1000000));
  connect(ui->mTerms, signal, this, [view](int value) {
    view->repo().appConfig().setValue("index.termlimit", value);
  });

  ui->mContext->setValue(config.value<int>("index.contextlines", 3));
  connect(ui->mContext, signal, this, [view](int value) {
    view->repo().appConfig().setValue("index.contextlines", value);
  });

  // Disable the limit controls when indexing is disabled.
  QList<QWidget *> widgets = {ui->mTerms,         ui->mTermsLabel,
                              ui->mTermsRowLabel, ui->mContext,
                              ui->mContextLabel,  ui->mContextRowLabel};

  auto setWidgetsEnabled = [widgets](bool enabled) {
    for (QWidget *widget : widgets)
      widget->setEnabled(enabled);
  };

  connect(ui->mEnable, &QCheckBox::toggled, setWidgetsEnabled);
  setWidgetsEnabled(ui->mEnable->isChecked());

  ui->mRemove->setEnabled(view->index()->isValid());
  connect(ui->mRemove, &QPushButton::clicked, this, [view, this] {
    Index *index = view->index();
    view->indexer()->cancel();
    index->remove();
    ui->mRemove->setEnabled(index->isValid());
  });
}

SearchPanel::~SearchPanel() = default;
