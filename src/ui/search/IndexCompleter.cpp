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

#include "IndexCompleter.h"
#include "SearchField.h"
#include "index/Index.h"
#include "ui/repo/RepoView.h"
#include "ui/window/MainWindow.h"
#include <QEvent>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QRegularExpression>
#include <QStringListModel>
#include <QToolButton>

namespace {

const QString kButtonStyle = "QPushButton {"
                             "  text-align: left;"
                             "  padding: 3px 3px 3px 3px;"
                             "  background: palette(base);"
#ifndef Q_OS_MAC
                             "  border: 1px solid palette(mid);"
#endif
                             "  border-top: 1px solid palette(dark)"
                             "}"
                             "QPushButton:pressed {"
                             "  background: palette(dark)"
                             "}";

const QRegularExpression kWsRe("\\s");

QString word(const QString &text, int &index) {
  index = text.lastIndexOf(kWsRe, qMax(0, index - 1)) + 1;
  return text.mid(index, text.indexOf(kWsRe, index) - index);
}

// Only the first suggestions are looked up, since the index can be huge.
const int kMaxSuggestions = 100;

class Popup : public QListView {
  Q_OBJECT

public:
  Popup(QLineEdit *parent = nullptr) : QListView(parent) {
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setTextElideMode(Qt::ElideLeft);
    setUniformItemSizes(true);

    if (SearchField *field = qobject_cast<SearchField *>(parent)) {
      mButton = new QPushButton(tr("Show Advanced Search"), this);
      mButton->setStyleSheet(kButtonStyle);
      mButton->setFlat(true);
      connect(mButton, &QPushButton::clicked, [this, field] {
        hide();
        field->advancedButton()->click();
      });

      setViewportMargins(0, 0, 0, mButton->sizeHint().height());
    }
  }

  void adjustLayout(int maxVisibleItems) {
    if (!mButton)
      return;

    int height = mButton->sizeHint().height();
    int y = sizeHintForRow(0) * qMin(maxVisibleItems, model()->rowCount()) + 4;
    mButton->setGeometry(0, y, width(), height);
    setMinimumHeight(y + height);
  }

private:
  QPushButton *mButton = nullptr;
};

} // namespace

IndexCompleter::IndexCompleter(MainWindow *window, QLineEdit *parent)
    : IndexCompleter(
          [window](const QString &prefix) {
            if (!window->count())
              return QStringList();
            Index *index = window->currentView()->index();
            return index->suggestions(Index::Any, prefix, kMaxSuggestions);
          },
          parent) {}

IndexCompleter::IndexCompleter(const Source &source, QLineEdit *parent)
    : QCompleter(parent) {
  QStringListModel *model = new QStringListModel(this);
  setModel(model);
  setCaseSensitivity(Qt::CaseInsensitive);

  Popup *popup = new Popup(parent);
  setPopup(popup);

  // Watch for popup show events.
  popup->installEventFilter(this);

  // Look up suggestions for the word at the cursor as it's typed. Edits are
  // signalled before the completer filters the model.
  connect(parent, &QLineEdit::textEdited, this, [parent, model, source] {
    int pos = parent->cursorPosition();
    QString prefix = word(parent->text(), pos);
    model->setStringList(prefix.isEmpty() ? QStringList() : source(prefix));
  });
}

bool IndexCompleter::eventFilter(QObject *watched, QEvent *event) {
  if (event->type() == QEvent::Show)
    mPos = -1;

  return QCompleter::eventFilter(watched, event);
}

QString IndexCompleter::pathFromIndex(const QModelIndex &index) const {
  QLineEdit *field = static_cast<QLineEdit *>(parent());
  if (!field->isEnabled())
    return QString();

  if (mPos < 0)
    mPos = field->cursorPosition();

  QString text = field->text();
  int len = word(text, mPos).length();
  return text.replace(mPos, len, index.data(completionRole()).toString());
}

QStringList IndexCompleter::splitPath(const QString &path) const {
  QLineEdit *field = static_cast<QLineEdit *>(parent());
  if (!field->isEnabled())
    return QStringList();

  // This is an egregious hack, but there doesn't
  // seem to be any reasonable place to hook in.
  static_cast<Popup *>(popup())->adjustLayout(maxVisibleItems());

  int pos = field->cursorPosition();
  QString term = word(path, pos);
  return QStringList(!term.isEmpty() ? term : path);
}

#include "IndexCompleter.moc"
