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

#ifndef INDEXCOMPLETER_H
#define INDEXCOMPLETER_H

#include <QCompleter>

class MainWindow;
class QLineEdit;

class IndexCompleter : public QCompleter {
public:
  IndexCompleter(MainWindow *window, QLineEdit *parent);
  IndexCompleter(QAbstractItemModel *model, QLineEdit *parent);

  bool eventFilter(QObject *watched, QEvent *event) override;
  QString pathFromIndex(const QModelIndex &index) const override;
  QStringList splitPath(const QString &path) const override;

private:
  mutable int mPos = -1;
};

#endif
