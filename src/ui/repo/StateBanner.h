//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#ifndef STATEBANNER_H
#define STATEBANNER_H

#include <QFrame>
#include <QList>
#include <functional>

class QHBoxLayout;
class QLabel;

// A strip that tells the user what needs attention in a repository, with the
// actions that resolve it.
class StateBanner : public QFrame {
  Q_OBJECT

public:
  struct Action {
    QString text;
    std::function<void()> run;
    bool enabled = true;
  };

  // Warning for something that blocks the user, Info for a next step.
  enum class Tone { Warning, Info };

  StateBanner(QWidget *parent = nullptr);

  // Shows the headline (in bold), the detail and the actions, or hides the
  // banner if there is no headline.
  void setState(const QString &headline, const QString &detail = QString(),
                const QList<Action> &actions = {}, Tone tone = Tone::Warning);

  // The headline and detail as plain text.
  QString message() const;

  // Two sentences in a row, as not every language separates them by a space.
  static QString joinSentences(const QString &first, const QString &second);

private:
  QLabel *mIcon;
  QLabel *mMessage;
  QString mPlain;
  QHBoxLayout *mActions;
};

#endif
