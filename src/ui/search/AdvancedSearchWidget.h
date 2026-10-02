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

#ifndef ADVANCEDSEARCHWIDGET_H
#define ADVANCEDSEARCHWIDGET_H

#include "index/Index.h"
#include <QComboBox>
#include <QFutureWatcher>
#include <QWidget>

class QFormLayout;
class QLineEdit;

class AdvancedSearchWidget : public QWidget {
  Q_OBJECT

public:
  AdvancedSearchWidget(QWidget *parent = nullptr);

  void exec(QLineEdit *parent, Index *index);

signals:
  void accepted(const QString &query);

protected:
  void hideEvent(QHideEvent *event) override;

private:
  void accept();

  void addLine(QFormLayout *layout);
  void addField(int field, const QString &text, const QString &tooltip);
  void addDateField(int field, const QString &text, const QString &tooltip);
  void addField(QWidget *widget, QLineEdit *lineEdit, int field,
                const QString &text, const QString &tooltip);

  QList<QLineEdit *> mLineEdits;
  QFutureWatcher<QMap<Index::Field, QStringList>> mFieldMapWatcher;
};

#endif
