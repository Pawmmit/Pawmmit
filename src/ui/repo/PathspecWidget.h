//
//          Copyright (c) 2017, Scientific Toolworks, Inc.
//
// This software is licensed under the GNU General Public License v3.0 or
// (at your option) any later version. The LICENSE.md file describes the
// conditions under which this software may be distributed.
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Author: Jason Haslam
//

#ifndef PATHSPECWIDGET_H
#define PATHSPECWIDGET_H

#include <QFrame>

class QAction;
class QLineEdit;
class QTreeView;

namespace git {
class Repository;
}

class PathspecWidget : public QFrame {
  Q_OBJECT

public:
  PathspecWidget(const git::Repository &repo, QWidget *parent = nullptr);

  QString pathspec() const;
  void setPathspec(const QString &pathspec);

signals:
  void pathspecChanged(const QString &pathspec);

private:
  QLineEdit *mField;
  QTreeView *mView;
  QAction *mIcon = nullptr;
  bool mHighlighted = false;
};

#endif
