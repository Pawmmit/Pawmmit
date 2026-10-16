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

#ifndef TABWIDGET_H
#define TABWIDGET_H

#include <QTabWidget>

class TabWidget : public QTabWidget {
  Q_OBJECT

public:
  TabWidget(QWidget *parent = nullptr);

signals:
  void tabAboutToBeInserted();
  void tabAboutToBeRemoved();
  void tabInserted();
  void tabRemoved();

protected:
  void resizeEvent(QResizeEvent *event) override;
  void tabInserted(int index) override;
  void tabRemoved(int index) override;

private:
  QWidget *mDefaultWidget;
};

#endif
