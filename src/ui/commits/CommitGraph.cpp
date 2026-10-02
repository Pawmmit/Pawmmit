//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "CommitGraph.h"
#include "Debug.h"
#include <QMap>

namespace graph {

// FIXME: Factor out into theme?
const QColor kTaintedColor = Qt::gray;

namespace {

int indexOf(const QList<Parent> &parents, const git::Commit &commit) {
  int count = parents.size();
  for (int i = 0; i < count; ++i) {
    if (parents.at(i).commit == commit)
      return i;
  }

  return -1;
}

bool contains(const git::Commit &commit, const QList<Row> &existingRows,
              const QList<Row> &newRows) {
  for (const Row &row : existingRows) {
    if (row.commit == commit)
      return true;
  }

  for (const Row &row : newRows) {
    if (row.commit == commit)
      return true;
  }

  return false;
}

} // namespace

QColor nextColor(const QList<Parent> &parents, const QList<QColor> &colors) {
  // Get the first unused (or least used) color.
  QMap<QString, int> counts;
  for (const Parent &parent : parents)
    counts[parent.color.name()]++;

  int count = 0;
  forever {
    for (const QColor &color : colors) {
      if (counts.value(color.name()) == count)
        return color;
    }

    ++count;
  }

  Q_UNREACHABLE();
  return QColor();
}

// The commit and parents parameters represent the current row.
// The nextParents parameter represents the next row after this one.
QVector<Column> columns(const git::Commit &commit, const QList<Parent> &parents,
                        const QList<Parent> &nextParents, bool root) {
  int count = parents.size();
  QVector<Column> columns(count);

  // Add incoming paths.
  int incoming = root ? count - 1 : count;
  for (int i = 0; i < incoming; ++i)
    columns[i] << Segment(Top, parents.at(i).taintedColor());

  // Add outgoing paths.
  for (int i = 0; i < count; ++i) {
    // Get the successors of this column.
    QList<git::Commit> successors;
    const Parent &parent = parents.at(i);
    if (parent.commit == commit) {
      successors = parent.commit.parents();
    } else {
      successors.append(parent.commit);
    }

    // Add a path to each successor.
    for (const git::Commit &successor : successors) {
      // Find index of parent in next row.
      int index = indexOf(nextParents, successor);
      if (index < 0)
        continue;

      // Handle multiple commits that share the same parent.
      bool single = (successors.size() == 1);
      const QColor &color =
          single ? parent.taintedColor(commit) : nextParents.at(index).color;

      if (index < i) {
        // out to the left
        columns[index] << Segment(RightIn, color);
        for (int j = index + 1; j < i; ++j)
          columns[j] << Segment(Cross, color);
        columns[i] << Segment(LeftOut, color);

      } else if (index > i) {
        // out to the right
        columns[i] << Segment(RightOut, color);
        for (int j = i + 1; j < index; ++j)
          columns[j] << Segment(Cross, color);
        if (index == columns.size())
          columns.append(Column());
        columns[index] << Segment(LeftIn, color);

      } else { // index == i
        // out the bottom
        columns[index] << Segment(Bottom, color);
      }
    }
  }

  // Add middle section last.
  for (int i = 0; i < count; ++i) {
    const Parent &parent = parents.at(i);
    bool dot = (parent.commit == commit);
    columns[i] << Segment(dot ? Dot : Middle, parent.taintedColor());
  }

  return columns;
}

FetchResult fetchRows(git::RevWalk &walker, QList<Parent> &parents,
                      const QList<Row> &existingRows, const QString &pathspec,
                      bool graphVisible, bool firstParentOnly,
                      const QList<QColor> &colors) {
  FetchResult result;
  int i = 0;
  git::Commit commit = walker.next(pathspec);
  while (commit.isValid()) {
    // Add root commits.
    bool root = false;
    if (indexOf(parents, commit) < 0) {
      root = true;
      parents.append(Parent(commit, nextColor(parents, colors)));
    }

    // Calculate graph columns.
    // Remember current row.
    QList<Parent> rowParents = parents;

    // Replace commit with its parents.
    QList<git::Commit> replacements;
    for (const git::Commit &parent : commit.parents()) {
      // FIXME: Mark commits that point to existing parent?
      if (indexOf(parents, parent) < 0 &&
          !contains(parent, existingRows, result.rows))
        replacements.append(parent);
      if (firstParentOnly) {
        break;
      }
    }

    // Set parents for next row.
    int index = indexOf(parents, commit);
    if (index >= 0) {
      Parent parent = parents.takeAt(index);
      if (!replacements.isEmpty()) {
        git::Commit replacement = replacements.takeFirst();
        parents.insert(index, Parent(replacement, parent.color));
        for (const git::Commit &replacement : replacements)
          parents.append(Parent(replacement, nextColor(parents, colors)));
      }
    }

    // Add graph row.
    QVector<Column> row;
    if (graphVisible && pathspec.isEmpty())
      row = columns(commit, rowParents, parents, root);

    result.rows.append(Row(commit, row));
    DebugRefresh("Append commit: " << commit.shortId());

    // Bail out.
    if (i++ >= 64)
      break;

    commit = walker.next(pathspec);
  }

  result.exhausted = !commit.isValid();
  return result;
}

} // namespace graph
