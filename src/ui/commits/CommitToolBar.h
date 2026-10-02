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

#ifndef COMMITTOOLBAR_H
#define COMMITTOOLBAR_H

#include <QToolBar>

class CommitToolBar : public QToolBar {
  Q_OBJECT

public:
  CommitToolBar(QWidget *parent = nullptr);

signals:
  void settingsChanged();
};

#endif
