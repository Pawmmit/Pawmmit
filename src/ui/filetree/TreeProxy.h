//
//
// This software is licensed under the GNU General Public License v3.0 or
// (at your option) any later version. The LICENSE.md file describes the
// conditions under which this software may be distributed.
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Author: Martin Marmsoler
//

#ifndef TREEPROXY_H
#define TREEPROXY_H

#include "git/Diff.h"
#include "git/Index.h"
#include "git/Repository.h"
#include "git/Tree.h"
#include <QFileIconProvider>
#include <QSortFilterProxyModel>

class QAbstractItemModel;
class TreeModel;

class TreeProxy : public QSortFilterProxyModel {
  Q_OBJECT

public:
  TreeProxy(bool staged, QAbstractItemModel *model, QObject *parent);
  virtual ~TreeProxy();
  bool setData(const QModelIndex &index, const QVariant &value,
               int role = Qt::EditRole, bool ignoreIndexChanges = false);
  bool staged() { return mStaged; }

  void enableFilter(bool enable) { mFilter = enable; }

  int columnCount(const QModelIndex &parent = QModelIndex()) const override {
    return sourceModel()->columnCount();
  }

private:
  using QSortFilterProxyModel::setData;
  bool filterAcceptsRow(int source_row,
                        const QModelIndex &source_parent) const override;
  bool mStaged{
      true}; // indicates, if only staged or only unstages files should be shown
  bool mFilter = true;
};

#endif // TREEPROXY_H
