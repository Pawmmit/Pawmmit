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

#ifndef MERGEDIALOG_H
#define MERGEDIALOG_H

#include "git/Repository.h"
#include "ui/repo/RepoView.h"
#include <QDialog>
#include <QScopedPointer>

class QPushButton;

namespace git {
class Reference;
}

namespace Ui {
class MergeDialog;
}

class MergeDialog : public QDialog {
  Q_OBJECT

public:
  MergeDialog(RepoView::MergeFlags flags, const git::Repository &repo,
              QWidget *parent = nullptr);
  ~MergeDialog() override;

  git::Commit target() const;
  git::Reference reference() const;
  RepoView::MergeFlags flags() const;

  void setCommit(const git::Commit &commit);
  void setReference(const git::Reference &ref);

private:
  void update();
  QString labelText() const;
  QString buttonText() const;

  git::Repository mRepo;
  QPushButton *mAccept;

  QScopedPointer<Ui::MergeDialog> ui;
};

#endif
