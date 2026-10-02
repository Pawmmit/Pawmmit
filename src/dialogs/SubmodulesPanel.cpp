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

#include "SubmodulesPanel.h"
#include "SubmoduleDelegate.h"
#include "SubmoduleTableModel.h"
#include "git/Submodule.h"
#include "ui/Footer.h"
#include "ui/repo/RepoView.h"
#include "ui_SubmodulesPanel.h"
#include <QHeaderView>

SubmodulesPanel::SubmodulesPanel(RepoView *view, QWidget *parent)
    : QWidget(parent), ui(new Ui::SubmodulesPanel) {
  ui->setupUi(this);

  ui->mTable->setModel(new SubmoduleTableModel(view->repo(), this));
  ui->mTable->setItemDelegate(new SubmoduleDelegate(ui->mTable));

  // Set section resize mode after model is set.
  ui->mTable->horizontalHeader()->setSectionResizeMode(
      SubmoduleTableModel::Name, QHeaderView::ResizeToContents);
  ui->mTable->horizontalHeader()->setSectionResizeMode(SubmoduleTableModel::Url,
                                                       QHeaderView::Stretch);

  connect(ui->mTable, &QTableView::doubleClicked, this,
          [view](const QModelIndex &index) {
            QVariant var = index.data(SubmoduleTableModel::SubmoduleRole);
            view->openSubmodule(var.value<git::Submodule>());
          });

  ui->mFooter->setPlusEnabled(false);
  ui->mFooter->setMinusEnabled(false);
}

SubmodulesPanel::~SubmodulesPanel() = default;
