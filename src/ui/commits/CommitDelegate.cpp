//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "CommitDelegate.h"
#include "CommitGraph.h"
#include "CommitList.h"
#include "app/Application.h"
#include "conf/Settings.h"
#include "git/Commit.h"
#include "git/Signature.h"
#include "ui/ProgressIndicator.h"
#include <QAbstractItemView>
#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QTextLayout>
#include <QtMath>

using namespace graph;

namespace {

// Use fixed short id size in compact mode.
// FIXME: Use 'core.abbrev' config instead?
const int kShortIdSize = 7;

} // namespace

CommitDelegate::CommitDelegate(const git::Repository &repo, QObject *parent)
    : QStyledItemDelegate(parent), mRepo(repo) {
  updateRefs();

  git::RepositoryNotifier *notifier = repo.notifier();
  connect(notifier, &git::RepositoryNotifier::referenceUpdated, this,
          &CommitDelegate::updateRefs);
  connect(notifier, &git::RepositoryNotifier::referenceAdded, this,
          &CommitDelegate::updateRefs);
  connect(notifier, &git::RepositoryNotifier::referenceRemoved, this,
          &CommitDelegate::updateRefs);
}

void CommitDelegate::paint(QPainter *painter,
                           const QStyleOptionViewItem &option,
                           const QModelIndex &index) const {
  QStyleOptionViewItem opt = option;
  initStyleOption(&opt, index);

  bool compact = Settings::instance()
                     ->value(Setting::Id::ShowCommitsInCompactMode)
                     .toBool();
  bool showAuthor = Settings::instance()
                        ->value(Setting::Id::ShowCommitsAuthor, true)
                        .toBool();
  bool showDate =
      Settings::instance()->value(Setting::Id::ShowCommitsDate, true).toBool();
  bool showId =
      Settings::instance()->value(Setting::Id::ShowCommitsId, true).toBool();
  LayoutConstants constants = layoutConstants(compact);

  bool active = (opt.state & QStyle::State_Active);
  bool selected = (opt.state & QStyle::State_Selected);
  auto group = active ? QPalette::Active : QPalette::Inactive;
  auto textRole = selected ? QPalette::HighlightedText : QPalette::Text;
  auto brightRole = selected ? QPalette::WindowText : QPalette::BrightText;
  QPalette palette = Application::theme()->commitList();
  QColor text = palette.color(group, textRole);
  QColor bright = palette.color(group, brightRole);
  QColor highlight = palette.color(group, QPalette::Highlight);

  painter->save();
  painter->setRenderHints(QPainter::Antialiasing);

  // Draw background.
  if (selected) {
    painter->fillRect(opt.rect, highlight);
  }

  // Draw busy indicator.
  if (opt.features & QStyleOptionViewItem::HasDecoration) {
    QRect rect = decorationRect(option, index);
    int progress = index.data(Qt::DecorationRole).toInt();
    ProgressIndicator::paint(painter, rect, bright, progress, opt.widget);
  }

  // Set default foreground color.
  painter->setPen(text);

  // Use default pen color for dot.
  QPen dot = painter->pen();
  dot.setWidth(2);

  // Copy content rect.
  QRect rect = opt.rect;
  rect.setX(rect.x() + 2);

  int totalWidth = rect.width();

  // Draw graph.
  painter->save();
  QVariantList columns = index.data(CommitList::Role::GraphRole).toList();
  QVariantList colorColumns =
      index.data(CommitList::Role::GraphColorRole).toList();
  for (int i = 0; i < columns.size(); ++i) {
    int x = rect.x();
    int y = rect.y();
    int w = opt.fontMetrics.ascent();
    int h = opt.rect.height();
    int h_2 = h / 2;
    int h_4 = h / 4;

    // radius
    int r = w / 3;

    // xs
    int x1 = x + (w / 2);
    int x2 = x + w;

    // ys
    int y1 = y + h_2 - r;
    int y2 = y + h_2;
    int y3 = y + h_2 + r;
    int y4 = y + h_2 + h_4;
    int y5 = y + h;

    QVariantList segments = columns.at(i).toList();
    QVariantList colors = colorColumns.at(i).toList();
    for (int j = 0; j < segments.size(); ++j) {
      QColor color = colors.at(j).value<QColor>();
      QPen pen(color, 2);
      if (color == kTaintedColor) {
        pen.setStyle(Qt::DashLine);
        pen.setDashPattern({2, 2});
      }

      painter->setPen(pen);
      switch (segments.at(j).toInt()) {
        case Dot:
          painter->setPen(dot);
          painter->drawEllipse(QPoint(x1, y2), r, r);
          break;

        case Top:
          painter->drawLine(x1, y, x1, y1);
          break;

        case Middle:
          painter->drawLine(x1, y1, x1, y3);
          break;

        case Bottom:
          painter->drawLine(x1, y3, x1, y5);
          break;

        case Cross:
          painter->drawLine(x, y4, x2, y4);
          break;

        case RightOut: {
          QPainterPath path;
          path.moveTo(x1, y3);
          path.quadTo(x1, y4, x2, y4);
          painter->drawPath(path);
          break;
        }

        case LeftOut: {
          QPainterPath path;
          path.moveTo(x1, y3);
          path.quadTo(x1, y4, x, y4);
          painter->drawPath(path);
          break;
        }

        case RightIn: {
          QPainterPath path;
          path.moveTo(x1, y5);
          path.quadTo(x1, y4, x2, y4);
          painter->drawPath(path);
          break;
        }

        case LeftIn: {
          QPainterPath path;
          path.moveTo(x1, y5);
          path.quadTo(x1, y4, x, y4);
          painter->drawPath(path);
          break;
        }
      }
    }

    rect.setX(x + w);

    // Finish early if the graph exceeds one third of the available space.
    if (rect.x() > opt.rect.width() / 3)
      break;
  }

  painter->restore();

  // Adjust margins.
  rect.setY(rect.y() + constants.vMargin);
  rect.setX(rect.x() + constants.hMargin);

  // Star has enough padding in compact mode.
  if (!compact)
    rect.setWidth(rect.width() - constants.hMargin);

  // Draw content.
  git::Commit commit =
      index.data(CommitList::Role::CommitRole).value<git::Commit>();
  if (!commit.isValid()) {
    // special case for uncommitted changes
    QString message = index.model()->data(index).toString();
    painter->save();
    QFont italic = opt.font;
    italic.setItalic(true);
    painter->setFont(italic);
    painter->drawText(opt.rect, Qt::AlignCenter, message);
    painter->restore();
  } else {
    const QFontMetrics &fm = opt.fontMetrics;
    QRect star = rect;

    QDateTime date = commit.committer().date().toLocalTime();
    QString timestamp =
        (date.date() == QDate::currentDate())
            ? QLocale().toString(date.time(), QLocale::ShortFormat)
            : QLocale().toString(date.date(), QLocale::ShortFormat);
    int timestampWidth = fm.horizontalAdvance(timestamp);

    if (compact) {
      int maxWidthRefs = rect.width() * 0.5; // Max 50%
      const int minWidthRefs = 50;           // At least display the ellipsis
      const int minWidthDesc = 100;
      int minDisplayWidthDate = 350;

      // Star always takes up its height on the right side.
      star.setX(star.x() + star.width() - star.height());
      star.setY(star.y() - constants.vMargin);
      rect.setWidth(rect.width() - star.width());

      // Draw commit id.
      if (showId) {
        QString id = commit.id().toString().left(kShortIdSize);
        int idWidth = maxShortIdWidth(fm);

        QRect commitRect = rect;
        commitRect.setX(commitRect.x() + commitRect.width() - idWidth);
        painter->save();
        painter->drawText(commitRect, Qt::AlignLeft, id);
        painter->restore();
        rect.setWidth(rect.width() - idWidth - constants.hMargin);
      }

      // Draw date. Only if it is not the same as previous?
      if (showDate && rect.width() > minWidthDesc + timestampWidth + 8 &&
          totalWidth > minDisplayWidthDate) {
        painter->save();
        painter->setPen(bright);
        painter->drawText(rect, Qt::AlignRight, timestamp);
        painter->restore();
        rect.setWidth(rect.width() - timestampWidth - constants.hMargin);
      }

      // Draw Name.
      if (showAuthor) {
        QString name = commit.author().name() + "  ";
        painter->save();
        QFont bold = opt.font;
        bold.setBold(true);
        painter->setFont(bold);
        painter->drawText(rect, Qt::AlignRight, name);
        painter->restore();
        const QFontMetrics boldFm(bold);
        rect.setWidth(rect.width() - boldFm.horizontalAdvance(name) -
                      constants.hMargin);
      }

      // Calculate remaining width for the references.
      QRect ref = rect;
      int refsWidth = ref.width() - minWidthDesc;
      if (maxWidthRefs <= minWidthRefs)
        maxWidthRefs = minWidthRefs;
      if (refsWidth < minWidthRefs)
        refsWidth = minWidthRefs;
      if (refsWidth > maxWidthRefs)
        refsWidth = maxWidthRefs;
      ref.setWidth(refsWidth);

      // Draw references.
      int badgesWidth = rect.x();
      QList<Badge::Label> refs = mRefs.value(commit.id());
      if (!refs.isEmpty())
        badgesWidth = Badge::paint(painter, refs, ref, &opt, Qt::AlignLeft);
      rect.setX(badgesWidth); // Comes right after the badges

      // Draw message.
      painter->save();
      painter->setPen(bright);
      QString msg = commit.summary(git::Commit::SubstituteEmoji);
      QString elidedText = fm.elidedText(msg, Qt::ElideRight, rect.width());
      painter->drawText(rect, Qt::ElideRight, elidedText);
      painter->restore();

    } else {

      // Draw Name.
      QString name = "";
      if (showAuthor) {
        name = commit.author().name();
        painter->save();
        QFont bold = opt.font;
        bold.setBold(true);
        painter->setFont(bold);
        painter->drawText(rect, Qt::AlignLeft, name);
        painter->restore();
      }

      // Draw date.
      if (showDate &&
          rect.width() > fm.horizontalAdvance(name) + timestampWidth + 8) {
        painter->save();
        painter->setPen(bright);
        if (showAuthor) {
          painter->drawText(rect, Qt::AlignRight, timestamp);
        } else {
          painter->drawText(rect, Qt::AlignLeft, timestamp);
        }
        painter->restore();
      }

      // Draw id.
      QString id = "";
      if (showId) {
        QRect idRect = rect;
        if (showAuthor || showDate) {
          idRect.setY(idRect.y() + constants.lineSpacing + constants.vMargin);
        }
        id = commit.shortId();
        painter->save();
        painter->drawText(idRect, Qt::AlignLeft, id);
        painter->restore();
      }

      // Draw references.
      QList<Badge::Label> refs = mRefs.value(commit.id());
      if (!refs.isEmpty()) {
        QRect refsRect = rect;
        QString leftText = "";

        if (showDate && showAuthor) {
          refsRect.setY(refsRect.y() + constants.lineSpacing +
                        constants.vMargin);
          if (showId) {
            leftText = id;
          }
        } else {
          if (showDate) {
            leftText = timestamp;
          } else if (showAuthor) {
            leftText = name;
          } else if (showId) {
            leftText = id;
          }
        }
        refsRect.setX(refsRect.x() + fm.boundingRect(leftText).width() + 6);
        Badge::paint(painter, refs, refsRect, &opt);
      }

      int numOptional = 0;
      if (showId)
        ++numOptional;
      if (showAuthor)
        ++numOptional;
      if (showDate)
        ++numOptional;
      if (numOptional > 1) {
        rect.setY(rect.y() + constants.lineSpacing + constants.vMargin);
      }

      rect.setY(rect.y() + constants.lineSpacing + constants.vMargin);

      // Divide remaining rectangle.
      star = rect;
      star.setX(star.x() + star.width() - star.height());
      QRect text = rect;
      text.setWidth(text.width() - star.width());

      // Draw message.
      painter->save();
      painter->setPen(bright);
      QString msg = commit.summary(git::Commit::SubstituteEmoji);
      QTextLayout layout(msg, painter->font());
      layout.beginLayout();

      QTextLine line = layout.createLine();
      if (line.isValid()) {
        int width = text.width();
        line.setLineWidth(width);
        int len = line.textLength();
        painter->drawText(text, Qt::AlignLeft, msg.left(len));

        if (len < msg.length()) {
          text.setY(text.y() + constants.lineSpacing);
          QString elided = fm.elidedText(msg.mid(len), Qt::ElideRight, width);
          painter->drawText(text, Qt::AlignLeft, elided);
        }
      }

      layout.endLayout();
      painter->restore();
    }

    // Draw star.
    bool starred = commit.isStarred();
    const QAbstractItemView *view =
        static_cast<const QAbstractItemView *>(opt.widget);
    QPoint pos = view->viewport()->mapFromGlobal(QCursor::pos());
    if (starred || (view->underMouse() && view->indexAt(pos) == index)) {
      painter->save();

      // Calculate outer radius and vertices.
      qreal r = (star.height() / 2.0) - constants.starPadding;
      qreal x = star.x() + (star.width() / 2.0);
      qreal y = star.y() + (star.height() / 2.0);
      qreal x1 = r * qCos(M_PI / 10.0);
      qreal y1 = -r * qSin(M_PI / 10.0);
      qreal x2 = r * qCos(17.0 * M_PI / 10.0);
      qreal y2 = -r * qSin(17.0 * M_PI / 10.0);

      // Calculate inner radius and vertices.
      qreal xi = ((y1 + r) * x2) / (y2 + r);
      qreal ri = qSqrt(qPow(xi, 2.0) + qPow(y1, 2.0));
      qreal xi1 = ri * qCos(3.0 * M_PI / 10.0);
      qreal yi1 = -ri * qSin(3.0 * M_PI / 10.0);
      qreal xi2 = ri * qCos(19.0 * M_PI / 10.0);
      qreal yi2 = -ri * qSin(19.0 * M_PI / 10.0);

      QPolygonF polygon({QPointF(0, -r), QPointF(xi1, yi1), QPointF(x1, y1),
                         QPointF(xi2, yi2), QPointF(x2, y2), QPointF(0, ri),
                         QPointF(-x2, y2), QPointF(-xi2, yi2), QPointF(-x1, y1),
                         QPointF(-xi1, yi1)});

      if (starred)
        painter->setBrush(Application::theme()->star());

      painter->setPen(QPen(bright, 1.25));
      painter->drawPolygon(polygon.translated(x, y));
      painter->restore();
    }
  }

  // Is the next index selected?
  bool nextSelected = false;

#ifndef Q_OS_WIN
  // Draw separator between selected indexes.
  QModelIndex next = index.sibling(index.row() + 1, 0);
  if (next.isValid()) {
    const QAbstractItemView *view =
        static_cast<const QAbstractItemView *>(opt.widget);
    nextSelected = view->selectionModel()->isSelected(next);
  }
#endif

  // Draw separator line.
  if (!compact && selected == nextSelected) {
    painter->save();
    painter->setRenderHints(QPainter::Antialiasing, false);
    painter->setPen(selected ? text : opt.palette.color(QPalette::Dark));
    painter->drawLine(rect.bottomLeft(), rect.bottomRight());
    painter->restore();
  }

  painter->restore();
}

QSize CommitDelegate::sizeHint(const QStyleOptionViewItem &option,
                               const QModelIndex &index) const {
  bool compact = Settings::instance()
                     ->value(Setting::Id::ShowCommitsInCompactMode)
                     .toBool();
  LayoutConstants constants = layoutConstants(compact);

  int lineHeight = constants.lineSpacing + constants.vMargin;
  return QSize(0, lineHeight * (compact ? 1 : 4));
}

QRect CommitDelegate::decorationRect(const QStyleOptionViewItem &option,
                                     const QModelIndex &index) const {
  QStyleOptionViewItem opt = option;
  initStyleOption(&opt, index);

  QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
  QStyle::SubElement se = QStyle::SE_ItemViewItemDecoration;
  return style->subElementRect(se, &opt, opt.widget);
}

QRect CommitDelegate::starRect(const QStyleOptionViewItem &option,
                               const QModelIndex &index) const {
  bool compact = Settings::instance()
                     ->value(Setting::Id::ShowCommitsInCompactMode)
                     .toBool();
  LayoutConstants constants = layoutConstants(compact);

  QRect rect = option.rect;
  int length = constants.lineSpacing * 2;
  rect.setX(rect.x() + rect.width() - length);
  rect.setY(rect.y() + rect.height() - length);
  rect.setWidth(rect.width() - constants.starPadding);
  rect.setHeight(rect.height() - constants.starPadding);
  return rect;
}

void CommitDelegate::initStyleOption(QStyleOptionViewItem *option,
                                     const QModelIndex &index) const {
  QStyledItemDelegate::initStyleOption(option, index);
  if (index.data(Qt::DecorationRole).canConvert<int>())
    option->decorationSize = ProgressIndicator::size();
}

CommitDelegate::LayoutConstants
CommitDelegate::layoutConstants(bool compact) const {
  return {compact ? 7 : 8, compact ? 23 : 16, compact ? 5 : 2, 4};
}

void CommitDelegate::updateRefs() {
  mRefs.clear();

  if (mRepo.isHeadDetached()) {
    git::Reference head = mRepo.head();
    mRefs[head.target().id()].append(
        {Badge::Label::Type::Ref, head.name(), true});
  }

  for (const git::Reference &ref : mRepo.refs()) {
    if (git::Commit target = ref.target())
      mRefs[target.id()].append(
          {Badge::Label::Type::Ref, ref.name(), ref.isHead(), ref.isTag()});
  }
}

int CommitDelegate::maxShortIdWidth(const QFontMetrics &fm) const {
  if (mMaxShortIdWidth < 0) {
    for (char ch = 'a'; ch <= 'f'; ++ch) {
      int width = fm.boundingRect(QString(kShortIdSize, ch)).width();
      mMaxShortIdWidth = qMax(mMaxShortIdWidth, width);
    }

    for (char ch = '0'; ch <= '9'; ++ch) {
      int width = fm.boundingRect(QString(kShortIdSize, ch)).width();
      mMaxShortIdWidth = qMax(mMaxShortIdWidth, width);
    }
  }

  return mMaxShortIdWidth;
}
