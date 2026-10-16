//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#ifndef COMMITGRAPH_H
#define COMMITGRAPH_H

#include "git/Commit.h"
#include "git/RevWalk.h"
#include <QColor>
#include <QList>
#include <QVector>

// Lays out the branch graph shown next to each row of the commit list.
namespace graph {

enum GraphSegment {
  Dot,
  Top,
  Middle,
  Bottom,
  Cross,
  LeftIn,
  LeftOut,
  RightIn,
  RightOut
};

// Color of the lines leading to the uncommitted changes.
extern const QColor kTaintedColor;

struct Parent {
  Parent(const git::Commit &commit, const QColor &color, bool tainted = false)
      : commit(commit), color(color), tainted(tainted) {}

  QColor taintedColor(const git::Commit &commit = git::Commit()) const {
    return (tainted && this->commit != commit) ? kTaintedColor : color;
  }

  git::Commit commit;
  QColor color;
  bool tainted;
};

struct Segment {
  Segment(GraphSegment segment, QColor color)
      : segment(segment), color(color) {}

  GraphSegment segment;
  QColor color;
};

using Column = QList<Segment>;

struct Row {
  Row(const git::Commit &commit, const QVector<Column> &columns)
      : commit(commit), columns(columns) {}

  git::Commit commit;
  QVector<Column> columns;
};

struct FetchResult {
  QList<Row> rows;
  bool exhausted = false;
};

// The first unused (or least used) of colors.
QColor nextColor(const QList<Parent> &parents, const QList<QColor> &colors);

// The graph columns of a row, given its parents and those of the next row.
QVector<Column> columns(const git::Commit &commit, const QList<Parent> &parents,
                        const QList<Parent> &nextParents, bool root);

// Walk at most one page of commits, updating parents in place and returning
// the new rows. It only touches its arguments, so it's safe on any thread.
FetchResult fetchRows(git::RevWalk &walker, QList<Parent> &parents,
                      const QList<Row> &existingRows, const QString &pathspec,
                      bool graphVisible, bool firstParentOnly,
                      const QList<QColor> &colors);

} // namespace graph

#endif
