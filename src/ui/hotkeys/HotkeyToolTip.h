//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#ifndef HOTKEYTOOLTIP_H
#define HOTKEYTOOLTIP_H

#include "HotkeyManager.h"
#include <QObject>

// Gives a widget a tool tip and accessible name from the same text, and adds
// the current binding of a hotkey to the tool tip if it has one.
class HotkeyToolTip : public QObject {
  Q_OBJECT

public:
  // The hotkey has to outlive the widget, as it is only referenced.
  HotkeyToolTip(QWidget *widget, const QString &text,
                const Hotkey &hotkey = Hotkey());

  void setText(const QString &text);

  // Extra lines below the text, e.g. why the widget is disabled.
  void setDetail(const QString &detail);

  // The tool tip of widget, if it was given one.
  static HotkeyToolTip *of(QWidget *widget);

private:
  void update();

  QWidget *mWidget;
  QString mText;
  QString mKeys;
  QString mDetail;
};

#endif
