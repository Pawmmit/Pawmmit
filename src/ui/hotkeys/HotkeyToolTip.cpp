//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "HotkeyToolTip.h"
#include <QKeySequence>
#include <QWidget>

HotkeyToolTip::HotkeyToolTip(QWidget *widget, const QString &text,
                             const Hotkey &hotkey)
    : QObject(widget), mWidget(widget) {
  if (hotkey.isValid()) {
    hotkey
        .use([this](const QKeySequence &keys) {
          mKeys = keys.toString(QKeySequence::NativeText);
          update();
        })
        ->setParent(this);
  }

  setText(text);
}

void HotkeyToolTip::setText(const QString &text) {
  mText = text;
  mWidget->setAccessibleName(text);
  update();
}

void HotkeyToolTip::setDetail(const QString &detail) {
  mDetail = detail;
  update();
}

HotkeyToolTip *HotkeyToolTip::of(QWidget *widget) {
  return widget->findChild<HotkeyToolTip *>(QString(),
                                            Qt::FindDirectChildrenOnly);
}

void HotkeyToolTip::update() {
  QString tip = mKeys.isEmpty() ? mText : QString("%1 (%2)").arg(mText, mKeys);
  mWidget->setToolTip(mDetail.isEmpty() ? tip : tip + "\n" + mDetail);
}
