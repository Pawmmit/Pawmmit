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

#ifndef FILECONTEXTMENU_H
#define FILECONTEXTMENU_H

#include "git/Commit.h"
#include "git/Id.h"
#include "git/Index.h"
#include <QMenu>

class ExternalTool;
class RepoView;

class FileContextMenu : public QMenu {
  Q_OBJECT

public:
  FileContextMenu(RepoView *view, const QStringList &files,
                  const git::Index &index = git::Index(),
                  QWidget *parent = nullptr);

  QAction *doubleClickAction() { return mDoubleClickAction; }

  // Start the external merge tool for a conflicted file, explaining why if it
  // can't.
  static void startMergeTool(RepoView *view, const QString &file,
                             QWidget *parent);

private slots:
  void ignoreFile();

private:
  QAction *addExternalToolsAction(const QList<ExternalTool *> &tools);
  bool exportFile(const RepoView *view, const QString &folder,
                  const QString &file);
  void handleUncommittedChanges(const git::Index &index,
                                const QStringList &files);
  void handleCommits(const QList<git::Commit> &commits,
                     const QStringList &files);

  RepoView *mView;
  QStringList mFiles;
  QAction *mDoubleClickAction;

  friend class TestTreeView;
};

#endif
