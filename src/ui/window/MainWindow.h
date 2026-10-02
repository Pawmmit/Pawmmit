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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "git/Repository.h"
#include <QMainWindow>
#include <functional>

class RepoView;
class TabWidget;
class QLabel;
class ToolBar;
class MenuBar;

namespace git {
class Submodule;
}

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  MainWindow(const git::Repository &repo, QWidget *parent = nullptr,
             Qt::WindowFlags flags = Qt::WindowFlags());

  ToolBar *toolBar() const { return mToolBar; }

  bool isSideBarVisible() const;
  void setSideBarVisible(bool visible);

  TabWidget *tabWidget() const;
  RepoView *addTab(const QString &path);
  RepoView *addTab(const git::Repository &repo);

  int count() const;
  RepoView *currentView() const;
  RepoView *view(int index) const;

  // Get the "active" main window.
  static MainWindow *activeWindow();
  static QList<MainWindow *> windows();

  // Restore previous open window state.
  // Returns true if any windows were opened.
  static bool restoreWindows();

  // Open a new window.
  static MainWindow *open(const QString &path, bool warnOnInvalid = true);
  static MainWindow *open(const git::Repository &repo = git::Repository());

  // How many commits a branch has to push or pull, in words.
  static QString commitsToPush(int count);
  static QString commitsToPull(int count);

  // Ask for a repository directory and open it, or pass it to onSelected.
  static void
  promptToOpen(QWidget *parent = nullptr,
               std::function<void(const QString &)> onSelected = nullptr);

  // Show the clone or initialize dialog and open the resulting repository,
  // using opener instead of MainWindow::open if given.
  static void
  promptToClone(QWidget *parent = nullptr,
                std::function<MainWindow *(const QString &)> opener = nullptr);
  static void
  promptToInit(QWidget *parent = nullptr,
               std::function<MainWindow *(const QString &)> opener = nullptr);

  // Save window settings on close.
  static void setSaveWindowSettings(bool enabled);

protected:
  void showEvent(QShowEvent *event) override;
  void closeEvent(QCloseEvent *event) override;
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dropEvent(QDropEvent *event) override;

private:
  void updateTabNames();
  void updateInterface();
  void updateWindowTitle(int ahead = -1, int behind = -1);
  void updateStatusBar(const git::Repository &repo, int ahead, int behind);

  // Returns true if the user chose to initialize a repository at path.
  static bool warnInvalidRepo(const QString &path);

  QStringList paths() const;
  QString windowGroup() const;

  ToolBar *mToolBar;
  MenuBar *mMenuBar;
  QLabel *mBranchLabel = nullptr;
  QLabel *mUpstreamLabel = nullptr;
  QLabel *mSyncLabel = nullptr;
  QWidget *mUpstreamSeparator = nullptr;
  QWidget *mSyncSeparator = nullptr;

  bool mFullPath = false;
  bool mIsSideBarVisible = true;

  bool mShown = false;
  bool mClosing = false;

  static bool sSaveWindowSettings;
};

#endif
