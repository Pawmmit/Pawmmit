//
//          Copyright (c) 2018, Scientific Toolworks, Inc.
//
// This software is licensed under the GNU General Public License v3.0 or
// (at your option) any later version. The LICENSE.md file describes the
// conditions under which this software may be distributed.
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Author: Jason Haslam
//

#ifndef SIDEBAR_H
#define SIDEBAR_H

#include <QWidget>

class Account;
class TabWidget;
class MainWindow;

class SideBar : public QWidget {
  Q_OBJECT

public:
  SideBar(TabWidget *tabs, MainWindow *mainWindow, QWidget *parent = nullptr);

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

private:
  void promptToRemoveAccount(Account *account);
};

#endif
