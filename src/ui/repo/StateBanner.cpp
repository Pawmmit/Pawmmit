//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "StateBanner.h"
#include "app/Application.h"
#include <QAccessible>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>

StateBanner::StateBanner(QWidget *parent) : QFrame(parent) {
  setObjectName("StateBanner");
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  setVisible(false);

  Theme *theme = Application::theme();
  QColor background = theme->notice(Theme::Notice::Background);
  QColor foreground = theme->notice(Theme::Notice::Foreground);
  setStyleSheet(QString("#StateBanner { background-color: %1; }"
                        "#StateBanner QLabel { color: %2; }")
                    .arg(background.name(), foreground.name()));

  mIcon = new QLabel(this);

  // Wrapping keeps a long message from widening the whole window.
  mMessage = new QLabel(this);
  mMessage->setWordWrap(true);

  mActions = new QHBoxLayout;
  mActions->setSpacing(8);

  QHBoxLayout *layout = new QHBoxLayout(this);
  layout->setContentsMargins(12, 6, 12, 6);
  layout->setSpacing(8);
  layout->addWidget(mIcon);
  layout->addWidget(mMessage, 1);
  layout->addLayout(mActions);
}

void StateBanner::setState(const QString &headline, const QString &detail,
                           const QList<Action> &actions, Tone tone) {
  bool wasVisible = isVisible();
  QString bold = QString("<b>%1</b>").arg(headline.toHtmlEscaped());
  mPlain = detail.isEmpty() ? headline : joinSentences(headline, detail);
  mMessage->setText(
      detail.isEmpty() ? bold : joinSentences(bold, detail.toHtmlEscaped()));

  QStyle::StandardPixmap icon = (tone == Tone::Warning)
                                    ? QStyle::SP_MessageBoxWarning
                                    : QStyle::SP_MessageBoxInformation;
  mIcon->setPixmap(style()->standardIcon(icon).pixmap(20, 20));

  // The state can change from inside a button's own click, so let go of the
  // old buttons later.
  while (QLayoutItem *item = mActions->takeAt(0)) {
    item->widget()->hide();
    item->widget()->deleteLater();
    delete item;
  }

  for (const Action &action : actions) {
    QPushButton *button = new QPushButton(action.text, this);
    button->setEnabled(action.enabled);
    connect(button, &QPushButton::clicked, this, [run = action.run] { run(); });
    mActions->addWidget(button);
  }

  setVisible(!headline.isEmpty());

  // Tell screen readers when something new needs attention.
  if (isVisible() && !wasVisible)
    QAccessible::updateAccessibility(
        new QAccessibleEvent(this, QAccessible::Alert));
}

QString StateBanner::message() const { return mPlain; }

QString StateBanner::joinSentences(const QString &first,
                                   const QString &second) {
  return tr("%1 %2", "Two sentences in a row").arg(first, second);
}
