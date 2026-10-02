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

#ifndef SEARCHFIELD_H
#define SEARCHFIELD_H

#include <QLineEdit>

class QResizeEvent;
class QToolButton;

class SearchField : public QLineEdit {
  Q_OBJECT

public:
  SearchField(QWidget *parent = nullptr);

  QToolButton *advancedButton() const { return mAdvancedButton; }

  QSize sizeHint() const override;

protected:
  void resizeEvent(QResizeEvent *) override;

private:
  QToolButton *mClearButton;
  QToolButton *mAdvancedButton = nullptr;
};

#endif
