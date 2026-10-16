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

#ifndef EDITORWINDOW_H
#define EDITORWINDOW_H

#include "git/Blob.h"
#include "git/Commit.h"
#include "git/Repository.h"
#include <QMainWindow>

class BlameEditor;

class EditorWindow : public QMainWindow {
  Q_OBJECT

public:
  EditorWindow(const git::Repository &repo = git::Repository(),
               QWidget *parent = nullptr);

  BlameEditor *widget() const;

  void updateWindowTitle();

  static EditorWindow *open(const QString &path,
                            const git::Blob &blob = git::Blob(),
                            const git::Commit &commit = git::Commit(),
                            const git::Repository &repo = git::Repository());

protected:
  void showEvent(QShowEvent *event) override;
  void closeEvent(QCloseEvent *event) override;
};

#endif
