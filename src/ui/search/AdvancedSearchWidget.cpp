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

#include "AdvancedSearchWidget.h"
#include "IndexCompleter.h"
#include "app/Application.h"
#include "index/Index.h"
#include "index/Query.h"
#include <QApplication>
#include <QCalendarWidget>
#include <QComboBox>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScreen>
#include <QShortcut>

namespace {

const char *kFieldProp = "field";
// Only the first suggestions are looked up, since the index can be huge.
const int kMaxSuggestions = 100;
const QString kParenFmt = "(%1)";
const QString kFieldFmt = "%1:%2";

} // namespace

class DateEdit : public QComboBox {
public:
  DateEdit(QWidget *parent) : QComboBox(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setEditable(true);

    mCalendar = new QCalendarWidget(this);
    mCalendar->setWindowFlags(Qt::Popup);
    mCalendar->setVisible(false);

    connect(mCalendar, &QCalendarWidget::clicked, this,
            [this](const QDate &date) {
              setCurrentText(date.toString(Index::dateFormat()));
              hidePopup();
            });
  }

  void showPopup() override {
    mCalendar->setVisible(true);

    QPoint pos = mapToGlobal(rect().bottomLeft());
    QRect screen = QGuiApplication::screenAt(pos)->availableGeometry();

    QSize size = mCalendar->sizeHint();
    if (pos.x() + size.width() > screen.right())
      pos.setX(screen.right() - size.width());
    pos.setX(qMax(pos.x(), screen.left()));
    mCalendar->move(pos);
  }

  void hidePopup() override {
    mCalendar->setVisible(false);
    mCalendar->setSelectedDate(QDate::currentDate());
  }

private:
  QCalendarWidget *mCalendar;
};

AdvancedSearchWidget::AdvancedSearchWidget(QWidget *parent)
    : QWidget(parent, Qt::Popup) {
  setStyleSheet("QLabel { color: palette(bright-text) }");

  QFormLayout *layout = new QFormLayout(this);
  layout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

  // name, email, message
  addField(Index::Author, tr("Author:"), tr("Author name"));
  addField(Index::Email, tr("Email:"), tr("Author email"));
  addField(Index::Message, tr("Message:"), tr("Commit message"));
  addLine(layout);

  // date
  addDateField(Index::Date, tr("Date:"), tr("Specific commit date"));
  addDateField(Index::After, tr("After:"), tr("Commits after date"));
  addDateField(Index::Before, tr("Before:"), tr("Commits before date"));
  addLine(layout);

  // file, path, scope
  addField(Index::File, tr("File:"), tr("File name"));
  addField(Index::Path, tr("Path:"), tr("File path"));
  addField(
      Index::Scope, tr("Scope:"),
      tr("Text in the hunk header, usually the enclosing function or class"));

  addLine(layout);

  // addition, deletion
  addField(Index::Addition, tr("Addition:"), tr("Text in added lines"));
  addField(Index::Deletion, tr("Deletion:"), tr("Text in removed lines"));

  addLine(layout);

  // comment, string, identifier
  addField(Index::Comment, tr("Comment:"), tr("Source code comment"));
  addField(Index::String, tr("String:"), tr("Source code string literal"));
  addField(Index::Identifier, tr("Identifier:"), tr("Source code identifier"));

  QPushButton *searchButton = new QPushButton(tr("Search"), this);
  QHBoxLayout *buttonLayout = new QHBoxLayout;
  buttonLayout->addStretch();
  buttonLayout->addWidget(searchButton);
  layout->addRow(buttonLayout);
  connect(searchButton, &QPushButton::clicked, this,
          &AdvancedSearchWidget::accept);

  QShortcut *accept = new QShortcut(tr("Return"), this);
  connect(accept, &QShortcut::activated, this, &AdvancedSearchWidget::accept);
}

void AdvancedSearchWidget::exec(QLineEdit *parent, Index *index) {
  resize(parent->width(), sizeHint().height());
  move(parent->mapToGlobal(QPoint(0, parent->height() + 2)));

  // Load initial values from query.
  // FIXME: Phrase queries are lossy.
  QMap<Index::Field, QStringList> map;
  if (QueryRef query = Query::parseQuery(parent->text())) {
    for (const Index::Term &term : query->terms())
      map[term.field].append(term.text);
  }

  // Reset fields.
  for (QLineEdit *lineEdit : mLineEdits) {
    QVariant var = lineEdit->property(kFieldProp);
    Index::Field field = static_cast<Index::Field>(var.toInt());
    lineEdit->setText(map.value(field).join(' '));
  }

  mIndex = index;
  show();
}

void AdvancedSearchWidget::accept() {
  QStringList fields;
  for (QLineEdit *lineEdit : mLineEdits) {
    QString text = lineEdit->text();
    if (text.isEmpty())
      continue;

    // Enclose queries with embedded spaces in parentheses.
    if (text.contains(' ') && (!text.startsWith('"') || !text.endsWith('"')))
      text = kParenFmt.arg(text);

    QVariant var = lineEdit->property(kFieldProp);
    Index::Field field = static_cast<Index::Field>(var.toInt());
    fields.append(kFieldFmt.arg(Index::fieldName(field), text));
  }

  emit accepted(fields.join(' '));

  hide();
}

void AdvancedSearchWidget::addLine(QFormLayout *layout) {
  QFrame *line = new QFrame(this);
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);
  layout->addRow(line);
}

void AdvancedSearchWidget::addField(int field, const QString &text,
                                    const QString &tooltip) {
  QLineEdit *lineEdit = new QLineEdit(this);
  auto source = [this, field](const QString &prefix) {
    if (!mIndex)
      return QStringList();
    return mIndex->suggestions(static_cast<Index::Field>(field), prefix,
                               kMaxSuggestions);
  };
  lineEdit->setCompleter(new IndexCompleter(source, lineEdit));
  addField(lineEdit, lineEdit, field, text, tooltip);
}

void AdvancedSearchWidget::addDateField(int field, const QString &text,
                                        const QString &tooltip) {
  DateEdit *dateEdit = new DateEdit(this);
  addField(dateEdit, dateEdit->lineEdit(), field, text, tooltip);
}

void AdvancedSearchWidget::addField(QWidget *widget, QLineEdit *lineEdit,
                                    int field, const QString &text,
                                    const QString &tooltip) {
  lineEdit->setProperty(kFieldProp, field);
  lineEdit->setClearButtonEnabled(true);
  mLineEdits.append(lineEdit);

  QLabel *label = new QLabel(text, this);
  label->setToolTip(tooltip);
  label->setCursor(Qt::WhatsThisCursor);

  static_cast<QFormLayout *>(layout())->addRow(label, widget);
}
