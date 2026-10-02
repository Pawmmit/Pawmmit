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

#include "MainWindow.h"
#include "MenuBar.h"
#include "SideBar.h"
#include "TabWidget.h"
#include "ToolBar.h"
#include "conf/RecentRepositories.h"
#include "conf/Settings.h"
#include "dialogs/CloneDialog.h"
#include "git/Config.h"
#include "git/Repository.h"
#include "git/Submodule.h"
#include "qmap.h"
#include "ui/repo/RepoView.h"
#include "ui/search/AdvancedSearchWidget.h"
#include "ui/search/IndexCompleter.h"
#include "ui/search/SearchField.h"
#include "util/Debug.h"
#include "util/Path.h"
#include <QApplication>
#include <QCloseEvent>
#include <QCryptographicHash>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QStatusBar>
#include <QTimeLine>
#include <QToolButton>

namespace {

const int kDefaultWidth = 1200;
const int kDefaultHeight = 800;

const QString kPathKey = "path";
const QString kIndexKey = "index";
const QString kStateKey = "state";
const QString kActiveKey = "active";
const QString kSidebarKey = "sidebar";
const QString kGeometryKey = "geometry";
const QString kWindowsGroup = "windows";
const QString kLastGeometryKey = "lastGeometry";

class TabName {
public:
  TabName(const QString &path) : mPath(path) {}

  QString name() const { return mPath.section('/', -mSections); }

  void increment() { ++mSections; }
  int sections() const { return mSections; }
  void setSections(int sections) { mSections = sections; }

private:
  QString mPath;
  int mSections = 1;
};

void promptToCreate(CloneDialog::Kind kind, QWidget *parent,
                    std::function<MainWindow *(const QString &)> opener) {
  if (!parent)
    parent = MainWindow::activeWindow();

  CloneDialog *dialog = new CloneDialog(kind, parent);
  QObject::connect(dialog, &CloneDialog::accepted, dialog, [dialog, opener] {
    QString path = dialog->path();
    MainWindow *window = opener ? opener(path) : MainWindow::open(path);
    if (window)
      window->currentView()->addLogEntry(dialog->message(),
                                         dialog->messageTitle());
  });

  dialog->open();
}

} // namespace

bool MainWindow::sSaveWindowSettings = false;

MainWindow::MainWindow(const git::Repository &repo, QWidget *parent,
                       Qt::WindowFlags flags)
    : QMainWindow(parent, flags) {
  setAttribute(Qt::WA_DeleteOnClose);
  setUnifiedTitleAndToolBarOnMac(true);
  setAcceptDrops(true);

  // Create new menu bar for this window if there isn't a shared one.
  mMenuBar = MenuBar::instance(this);
  mMenuBar->registerActions(this);

  // Create tool bar.
  mToolBar = new ToolBar(this);
  addToolBar(Qt::TopToolBarArea, mToolBar);

  // Initialize search.
  SearchField *searchField = mToolBar->searchField();
  connect(searchField, &QLineEdit::textEdited, mMenuBar,
          &MenuBar::updateUndoRedo);
  connect(searchField, &QLineEdit::selectionChanged, mMenuBar,
          &MenuBar::updateCutCopyPaste);

  // Hook up advanced search.
  AdvancedSearchWidget *advancedSearch = new AdvancedSearchWidget(this);
  connect(advancedSearch, &AdvancedSearchWidget::accepted, searchField,
          &QLineEdit::setText);
  connect(searchField->advancedButton(), &QToolButton::clicked, this,
          [this, searchField, advancedSearch] {
            advancedSearch->exec(searchField, currentView()->index());
          });

  // Update title and refresh when settings change.
  mFullPath =
      Settings::instance()->value(Setting::Id::ShowFullRepoPath).toBool();
  connect(Settings::instance(), &Settings::settingsChanged, this,
          [this](bool refresh) {
            Settings *settings = Settings::instance();

            bool menuBarHidden =
                settings->value(Setting::Id::HideMenuBar).toBool();
            if (mMenuBar->isHidden() != menuBarHidden)
              mMenuBar->setHidden(menuBarHidden);

            bool fullPath =
                settings->value(Setting::Id::ShowFullRepoPath).toBool();
            if (mFullPath != fullPath) {
              mFullPath = fullPath;
              updateWindowTitle();
            }

            if (refresh) {
              for (int i = 0; i < count(); ++i)
                view(i)->refresh();
            }
          });

  // Create splitter.
  QSplitter *splitter = new QSplitter(this);
  splitter->setHandleWidth(0);
  connect(splitter, &QSplitter::splitterMoved, [this] {
    QSplitter *splitter = static_cast<QSplitter *>(centralWidget());
    mIsSideBarVisible = (splitter->sizes().first() > 0);
  });

  // Create tab container.
  TabWidget *tabs = new TabWidget(splitter);
  connect(tabs, &TabWidget::currentChanged, [this](int index) {
    updateInterface();
    MenuBar::instance(this)->update();
  });

  connect(tabs, QOverload<>::of(&TabWidget::tabInserted), this,
          &MainWindow::updateTabNames);
  connect(tabs, QOverload<>::of(&TabWidget::tabRemoved), this,
          &MainWindow::updateTabNames);

  splitter->addWidget(new SideBar(tabs, this, splitter));
  splitter->addWidget(tabs);
  splitter->setCollapsible(1, false);
  splitter->setStretchFactor(1, 1);

  setCentralWidget(splitter);

  // Show the branch and how it relates to its upstream, part by part so that
  // each can explain itself.
  auto addLabel = [this](const char *name) {
    QLabel *label = new QLabel(this);
    label->setObjectName(name);
    label->setContentsMargins(6, 0, 6, 0);
    statusBar()->addWidget(label);
    return label;
  };
  auto addSeparator = [this] {
    QFrame *line = new QFrame(this);
    line->setFrameShape(QFrame::VLine);
    line->setFrameShadow(QFrame::Sunken);
    statusBar()->addWidget(line);
    return line;
  };
  mBranchLabel = addLabel("BranchLabel");
  mUpstreamSeparator = addSeparator();
  mUpstreamLabel = addLabel("UpstreamLabel");
  mSyncSeparator = addSeparator();
  mSyncLabel = addLabel("SyncLabel");

  if (repo)
    addTab(repo);

  // Set search completer.
  searchField->setCompleter(new IndexCompleter(this, searchField));

  // Restore the last known size and position, falling back to a default.
  QByteArray lastGeometry = QSettings().value(kLastGeometryKey).toByteArray();
  if (!lastGeometry.isEmpty()) {
    restoreGeometry(lastGeometry);
  } else {
    resize(kDefaultWidth, kDefaultHeight);

    QRect desktop = QGuiApplication::primaryScreen()->availableGeometry();
    int x = (desktop.width() / 2) - (kDefaultWidth / 2);
    int y = (desktop.height() / 2) - (kDefaultHeight / 2);
    move(x, y);

    // Position with respect to existing windows.
    if (MainWindow *win = activeWindow())
      move(win->x() + 24, win->y() + 24);
  }

  // Restore sidebar.
  setSideBarVisible(QSettings().value(kSidebarKey, true).toBool());

  // Set initial state of interface.
  updateInterface();
}

bool MainWindow::isSideBarVisible() const { return mIsSideBarVisible; }

void MainWindow::setSideBarVisible(bool visible) {
  if (visible == mIsSideBarVisible)
    return;

  mIsSideBarVisible = visible;

  // Remember in settings.
  QSettings().setValue(kSidebarKey, visible);

  // Animate sidebar sliding in or out.
  QSplitter *splitter = static_cast<QSplitter *>(centralWidget());
  QWidget *sidebar = splitter->widget(0);
  int pos = visible ? sidebar->sizeHint().width() : splitter->sizes().first();

  QTimeLine *timeline = new QTimeLine(250, this);
  timeline->setDirection(visible ? QTimeLine::Forward : QTimeLine::Backward);
  timeline->setEasingCurve(QEasingCurve(QEasingCurve::Linear));
  timeline->setUpdateInterval(20);

  connect(timeline, &QTimeLine::valueChanged, [this, pos](qreal value) {
    QSplitter *splitter = static_cast<QSplitter *>(centralWidget());
    splitter->setSizes({static_cast<int>(pos * value), 1});
  });

  connect(timeline, &QTimeLine::finished,
          [timeline] { timeline->deleteLater(); });

  timeline->start();
}

TabWidget *MainWindow::tabWidget() const {
  QSplitter *splitter = static_cast<QSplitter *>(centralWidget());
  return static_cast<TabWidget *>(splitter->widget(1));
}

RepoView *MainWindow::addTab(const QString &path) {
  if (path.isEmpty())
    return nullptr;

  TabWidget *tabs = tabWidget();
  for (int i = 0; i < tabs->count(); i++) {
    RepoView *view = static_cast<RepoView *>(tabs->widget(i));
    if (path == view->repo().dir(false).path()) {
      tabs->setCurrentIndex(i);
      return view;
    }
  }

  git::Repository repo = git::Repository::open(path, true);
  if (!repo.isValid() && warnInvalidRepo(path))
    repo = git::Repository::open(path, true);

  if (!repo.isValid())
    return nullptr;

  return addTab(repo);
}

RepoView *MainWindow::addTab(const git::Repository &repo) {
  // Update recent repository settings.
  QDir dir = repo.dir(false);
  RecentRepositories::instance()->add(dir.path());

  TabWidget *tabs = tabWidget();
  for (int i = 0; i < tabs->count(); i++) {
    RepoView *view = static_cast<RepoView *>(tabs->widget(i));
    if (dir.path() == view->repo().dir(false).path()) {
      tabs->setCurrentIndex(i);
      return view;
    }
  }

  RepoView *view = new RepoView(repo, this);
  view->detailSplitterMaximize(mMenuBar->isMaximized());
  git::RepositoryNotifier *notifier = repo.notifier();
  connect(notifier, &git::RepositoryNotifier::referenceUpdated, this,
          &MainWindow::updateInterface);
  connect(notifier, &git::RepositoryNotifier::stateChanged, this,
          [this] { updateWindowTitle(); });

  emit tabs->tabAboutToBeInserted();
  tabs->setCurrentIndex(tabs->addTab(view, dir.dirName()));

  Settings *settings = Settings::instance();
  bool enable =
      settings->value(Setting::Id::UpdateSubmodulesAfterPullAndClone).toBool();
  if (repo.appConfig().value<bool>("autoupdate.enable", enable)) {
    // update submodules
    view->updateSubmodules(repo.submodules(), true, true, false, nullptr);
  }

  // Start status diff.
  view->refresh(false);
  return view;
}

int MainWindow::count() const { return tabWidget()->count(); }

RepoView *MainWindow::currentView() const {
  auto *view = static_cast<RepoView *>(tabWidget()->currentWidget());
  // Hide a view that's tearing down, since a focus change during its own
  // destruction can reach here before it's removed as the current tab.
  return (view && view->isDestroying()) ? nullptr : view;
}

RepoView *MainWindow::view(int index) const {
  return static_cast<RepoView *>(tabWidget()->widget(index));
}

MainWindow *MainWindow::activeWindow() {
  QWidget *win = QApplication::activeWindow();
  if (MainWindow *mainWin = qobject_cast<MainWindow *>(win))
    return mainWin;

  QList<MainWindow *> mainWins = windows();
  return !mainWins.isEmpty() ? mainWins.first() : nullptr;
}

QList<MainWindow *> MainWindow::windows() {
  QList<MainWindow *> mainWins;
  for (QWidget *win : QApplication::topLevelWidgets()) {
    if (MainWindow *mainWin = qobject_cast<MainWindow *>(win))
      mainWins.append(mainWin);
  }

  return mainWins;
}

bool MainWindow::restoreWindows() {
  QList<MainWindow *> windows;

  // Open windows.
  QSettings settings;
  settings.beginGroup(kWindowsGroup);
  for (const QString &group : settings.childGroups()) {
    settings.beginGroup(group);
    int index = settings.value(kIndexKey).toInt();
    bool active = settings.value(kActiveKey).toBool();
    QStringList paths = settings.value(kPathKey).toStringList();
    QByteArray state = settings.value(kStateKey).toByteArray();
    QByteArray geometry = settings.value(kGeometryKey).toByteArray();
    settings.endGroup();

    // This shouldn't ever happen.
    if (paths.isEmpty())
      continue;

    // Open a window new for the first valid repo.
    MainWindow *window = open(paths.takeFirst());
    while (!window && !paths.isEmpty())
      window = open(paths.takeFirst());

    if (!window)
      continue;

    // Add the remainder as tabs.
    for (const QString &path : paths)
      window->addTab(path);

    // Select saved index.
    window->tabWidget()->setCurrentIndex(index);

    // Restore state and geometry.
    window->restoreState(state);
    window->restoreGeometry(geometry);

    // Order active window first.
    windows.insert(active ? 0 : windows.size(), window);
  }
  settings.endGroup();

  // Remove all window settings.
  settings.remove(kWindowsGroup);

  // Activate the top window.
  if (!windows.isEmpty()) {
    MainWindow *window = windows.first();
    window->raise();
    window->activateWindow();
  }

  return !windows.isEmpty();
}

MainWindow *MainWindow::open(const QString &path, bool warnOnInvalid) {
  DebugRefresh("Open project: " << path);
  if (path.isEmpty())
    return nullptr;

  git::Repository repo = git::Repository::open(path, true);
  if (!repo.isValid() && warnOnInvalid && warnInvalidRepo(path))
    repo = git::Repository::open(path, true);

  if (!repo.isValid())
    return nullptr;

  if (Settings::instance()->value(Setting::Id::OpenAllReposInTabs).toBool()) {
    if (MainWindow *win = activeWindow()) {
      win->addTab(repo);
      return win;
    }
  }

  return open(repo);
}

MainWindow *MainWindow::open(const git::Repository &repo) {
  // Update recent repository settings.
  if (repo.isValid())
    RecentRepositories::instance()->add(repo.dir(false).path());

  // Create the window.
  MainWindow *window = new MainWindow(repo);

  const bool showMaximized =
      Settings::instance()->value(Setting::Id::ShowMaximized).toBool();

  if (showMaximized) {
    window->showMaximized();
  } else {
    window->show();
  }

  return window;
}

void MainWindow::promptToOpen(QWidget *parent,
                              std::function<void(const QString &)> onSelected) {
  Settings *settings = Settings::instance();
  QString start = settings->lastPath();
  if (start.isEmpty())
    start = QDir::homePath();

  QFileDialog *dialog = new QFileDialog(parent, tr("Open Repository"), start);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->setFileMode(QFileDialog::Directory);
  dialog->setOption(QFileDialog::ShowDirsOnly);
  connect(dialog, &QFileDialog::fileSelected, dialog,
          [settings, onSelected](const QString &path) {
            settings->setLastPath(path);
            if (onSelected) {
              onSelected(path);
            } else {
              MainWindow::open(path);
            }
          });

  dialog->open();
}

void MainWindow::promptToClone(
    QWidget *parent, std::function<MainWindow *(const QString &)> opener) {
  promptToCreate(CloneDialog::Clone, parent, std::move(opener));
}

void MainWindow::promptToInit(
    QWidget *parent, std::function<MainWindow *(const QString &)> opener) {
  promptToCreate(CloneDialog::Init, parent, std::move(opener));
}

void MainWindow::setSaveWindowSettings(bool enabled) {
  sSaveWindowSettings = enabled;
}

void MainWindow::showEvent(QShowEvent *event) {
  QMainWindow::showEvent(event);

  if (mShown)
    return;

  mShown = true;
  updateInterface();
}

void MainWindow::closeEvent(QCloseEvent *event) {
  // FIXME: Attempt to close windows before writing settings?

  // Remember size and position for the next new window, independent of
  // full session restore.
  QSettings().setValue(kLastGeometryKey, saveGeometry());

  if (sSaveWindowSettings) {
    // Store window state.
    // FIXME: Qt doesn't impose a predictable order on top-level windows.
    // Instead order the active window first and leave others undefined.
    QSettings settings;
    settings.beginGroup(kWindowsGroup);
    settings.beginGroup(windowGroup());
    settings.setValue(kPathKey, paths());
    settings.setValue(kIndexKey, tabWidget()->currentIndex());
    settings.setValue(kActiveKey, this == activeWindow());
    settings.setValue(kStateKey, saveState());
    settings.setValue(kGeometryKey, saveGeometry());
    settings.endGroup();
    settings.endGroup();
  }

  for (int i = 0; i < count(); ++i) {
    if (!view(i)->close()) {
      event->ignore();
      return;
    }
  }

  mClosing = true;
  QMainWindow::closeEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
  if (!event->mimeData()->hasFormat("text/uri-list"))
    return;

  for (const QUrl &url : event->mimeData()->urls()) {
    if (!url.isLocalFile())
      return;

    QDir dir(url.toLocalFile());
    if (!dir.exists())
      return;

    if (!git::Repository::open(dir.path(), true).isValid())
      return;
  }

  event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event) {
  for (const QUrl &url : event->mimeData()->urls())
    addTab(url.toLocalFile());
}

bool MainWindow::warnInvalidRepo(const QString &path) {
  QString title = tr("Invalid Git Repository");
  QString text = tr("%1 does not contain a valid git repository.");
  QMessageBox mb(QMessageBox::Warning, title, text.arg(path),
                 QMessageBox::Cancel);

  QPushButton *init = nullptr;
  if (QFileInfo(path).isDir())
    init = mb.addButton(tr("Initialize Repository"), QMessageBox::AcceptRole);

  mb.exec();
  return init && mb.clickedButton() == init &&
         git::Repository::init(path).isValid();
}

void MainWindow::updateTabNames() {
  TabWidget *tabs = tabWidget();
  QHash<QString, QList<int>> names;
  QList<TabName> fullNames;

  for (int i = 0; i < count(); ++i) {
    TabName name(view(i)->repo().dir(false).path());
    names[name.name()].append(i);
    fullNames.append(name);
  }

  QHash<QString, QList<int>>::key_iterator first;
  while ((first = names.keyBegin()) != names.keyEnd()) {
    auto key = *first;
    auto ids = names.take(key);

    if (ids.count() == 1) {
      tabs->setTabText(ids.first(), key);
    } else {
      for (auto id : ids) {
        auto &name = fullNames[id];
        name.increment();
        names[name.name()].append(id);
      }
    }
  }
}

void MainWindow::updateInterface() {
  // Avoid updating during close.
  if (mClosing)
    return;

  int ahead = 0;
  int behind = 0;
  if (RepoView *view = currentView()) {
    if (git::Branch head = view->repo().head()) {
      if (git::Branch upstream = head.upstream()) {
        ahead = head.difference(upstream);
        behind = upstream.difference(head);
      }
    }
  }

  updateWindowTitle(ahead, behind);
  mToolBar->updateButtons(ahead, behind);
}

QString MainWindow::commitsToPush(int count) {
  return tr("%n commit(s) to push", nullptr, count);
}

QString MainWindow::commitsToPull(int count) {
  return tr("%n commit(s) to pull", nullptr, count);
}

void MainWindow::updateStatusBar(const git::Repository &repo, int ahead,
                                 int behind) {
  QString branch, branchTip, upstream, upstreamTip, sync, syncTip;

  if (repo.isValid()) {
    git::Reference head = repo.head();
    if (!head.isValid()) {
      // No commits yet, so there is no branch to point at.
      branch = tr("On branch %1").arg(repo.unbornHeadName());
      branchTip = tr("The branch you are working on");
      upstream = tr("no commits yet");
      upstreamTip = tr("The branch is created by the first commit.");
    } else if (!head.isLocalBranch()) {
      branch = tr("Not on a branch (viewing %1)").arg(head.target().shortId());
      branchTip = tr("You're looking at a single commit rather than a "
                     "branch. Anything you commit here is easy to lose.");
    } else {
      branch = tr("On branch %1").arg(head.name());
      branchTip = tr("The branch you are working on");

      git::Branch upstreamBranch = git::Branch(head).upstream();
      if (upstreamBranch) {
        upstream = tr("linked to %1").arg(upstreamBranch.name());
        upstreamTip =
            tr("Pull and Push exchange commits with this remote branch");

        QStringList parts;
        if (ahead > 0)
          parts.append(commitsToPush(ahead));
        if (behind > 0)
          parts.append(commitsToPull(behind));
        sync =
            parts.isEmpty() ? tr("in sync as of last fetch") : parts.join(", ");
        syncTip = tr("Compared with the last fetch. Fetch to check for newer "
                     "commits.");
      } else {
        upstream = tr("only on this computer");
        upstreamTip = tr("This branch isn't on a remote yet. Push it to share "
                         "it and keep a copy off this computer.");
      }
    }
  }

  auto show = [](QLabel *label, QWidget *separator, const QString &text,
                 const QString &tip) {
    label->setText(text);
    label->setToolTip(tip);
    label->setAccessibleName(text);
    label->setVisible(!text.isEmpty());
    if (separator)
      separator->setVisible(!text.isEmpty());
  };
  show(mBranchLabel, nullptr, branch, branchTip);
  show(mUpstreamLabel, mUpstreamSeparator, upstream, upstreamTip);
  show(mSyncLabel, mSyncSeparator, sync, syncTip);
}

void MainWindow::updateWindowTitle(int ahead, int behind) {
  RepoView *view = currentView();
  if (!view) {
    setWindowTitle(QCoreApplication::applicationName() + BUILD_DESCRIPTION);
    updateStatusBar(git::Repository(), 0, 0);
    return;
  }

  git::Repository repo = view->repo();
  QDir dir = repo.dir(false);
  git::Reference head = repo.head();
  // Resolve potentially sandboxed path
  QString path =
      mFullPath ? util::sandboxPathToHost(dir.path()) : dir.dirName();
  QString name = head.isValid() ? head.name() : repo.unbornHeadName();
  QString summary = name;

  // Add remote tracking information.
  if (git::Branch branch = head) {
    if (git::Branch upstream = branch.upstream()) {
      if (ahead < 0)
        ahead = branch.difference(upstream);
      if (behind < 0)
        behind = upstream.difference(branch);

      QStringList parts;
      if (ahead > 0)
        parts.append(commitsToPush(ahead));
      if (behind > 0)
        parts.append(commitsToPull(behind));

      QString status = parts.isEmpty() ? tr("in sync") : parts.join(", ");
      QString remote = tr("%1 (%2)").arg(status, upstream.name());
      summary = tr("%1 - %2").arg(name, remote);
    }
  }

  updateStatusBar(repo, ahead, behind);
  QString title = tr("%1 - %2").arg(path, summary);

  // Add state.
  QString state;
  switch (repo.operation()) {
    case git::Operation::Merge:
      state = tr("MERGING");
      break;

    case git::Operation::Revert:
      state = tr("REVERTING");
      break;

    case git::Operation::CherryPick:
      state = tr("CHERRY-PICKING");
      break;

    case git::Operation::Rebase:
      state = tr("REBASING");
      break;

    case git::Operation::ApplyMailbox:
    case git::Operation::Bisect:
    case git::Operation::None:
      break; // FIXME?
  }

  if (!state.isEmpty())
    title = tr("%1 (%2)").arg(title, state);

  setWindowTitle(QString("%1%2").arg(title, BUILD_DESCRIPTION));
}

QStringList MainWindow::paths() const {
  QStringList paths;
  for (int i = 0; i < count(); ++i)
    paths.append(view(i)->repo().dir(false).path());
  return paths;
}

QString MainWindow::windowGroup() const {
  QByteArray group = paths().join(';').toUtf8();
  QByteArray hash = QCryptographicHash::hash(group, QCryptographicHash::Md5);
  return QString::fromUtf8(hash.toHex());
}
