//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#ifndef COMMITDELEGATE_H
#define COMMITDELEGATE_H

#include "git/Id.h"
#include "git/Repository.h"
#include "ui/Badge.h"
#include <QMap>
#include <QStyledItemDelegate>

// Paints a row of the commit list: graph, message, author, date and badges.
class CommitDelegate : public QStyledItemDelegate {
public:
  CommitDelegate(const git::Repository &repo, QObject *parent = nullptr);

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override;

  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override;

  QRect decorationRect(const QStyleOptionViewItem &option,
                       const QModelIndex &index) const;

  QRect starRect(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const;

protected:
  void initStyleOption(QStyleOptionViewItem *option,
                       const QModelIndex &index) const override;

private:
  struct LayoutConstants {
    const int starPadding;
    const int lineSpacing;
    const int vMargin;
    const int hMargin;
  };

  LayoutConstants layoutConstants(bool compact) const;

  void updateRefs();

  int maxShortIdWidth(const QFontMetrics &fm) const;

  git::Repository mRepo;
  QMap<git::Id, QList<Badge::Label>> mRefs;

  mutable int mMaxShortIdWidth = -1;
};

#endif
