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

#include "SettingsDialog.h"
#include "AboutDialog.h"
#include "DiffPanel.h"
#include "EditorPanel.h"
#include "ExternalToolsDialog.h"
#include "GeneralPanel.h"
#include "HotkeysPanel.h"
#include "MiscPanel.h"
#include "PluginsPanel.h"
#include "TerminalPanel.h"
#include "ToolsPanel.h"
#include "UpdatePanel.h"
#include "WindowPanel.h"
#include "app/Application.h"
#include "app/CustomTheme.h"
#include "conf/Settings.h"
#include "cred/CredentialHelper.h"
#include "git/Config.h"
#include "languages.h"
#include "log/LogEntry.h"
#include "tools/ExternalTool.h"
#include "ui/blame/BlameEditor.h"
#include "ui/repo/RepoView.h"
#include "ui/window/EditorWindow.h"
#include "ui/window/MainWindow.h"
#include "ui/window/MenuBar.h"
#include "update/Updater.h"
#include <QAction>
#include <QActionGroup>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QShortcut>
#include <QStackedWidget>
#include <QToolBar>
#include <QVBoxLayout>

// Named, not anonymous; kept file-local via the `using namespace` below.
namespace SettingsDialogPrivate {

// Size to the largest page rather than the current one, so the dialog
// settles at one size instead of resizing every time the panel changes.
class StackedWidget : public QStackedWidget {
public:
  StackedWidget(QWidget *parent = nullptr) : QStackedWidget(parent) {}

  QSize sizeHint() const override {
    QSize size;
    for (int i = 0; i < count(); ++i)
      size = size.expandedTo(widget(i)->sizeHint());
    return size;
  }

  QSize minimumSizeHint() const override {
    QSize size;
    for (int i = 0; i < count(); ++i)
      size = size.expandedTo(widget(i)->minimumSizeHint());
    return size;
  }
};

} // namespace SettingsDialogPrivate
using namespace SettingsDialogPrivate;

SettingsDialog::SettingsDialog(Index index, QWidget *parent)
    : QMainWindow(parent, Qt::Dialog) {
  setAttribute(Qt::WA_DeleteOnClose);
  setUnifiedTitleAndToolBarOnMac(true);
  setContextMenuPolicy(Qt::NoContextMenu);

  // Close on escape.
  QShortcut *esc = new QShortcut(tr("Esc"), this);
  connect(esc, &QShortcut::activated, this, &SettingsDialog::close);

  // Create tool bar.
  QToolBar *toolbar = new QToolBar(this);
  toolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  toolbar->setMovable(false);
  addToolBar(toolbar);

  // Create central stack widget.
  StackedWidget *stack = new StackedWidget(this);
  connect(stack, &StackedWidget::currentChanged, this,
          &SettingsDialog::adjustSize);

  QString text =
      tr("Global git settings can be overridden for each repository in "
         "the corresponding repository configuration page.");
  QLabel *description = new QLabel(text, this);
  description->setStyleSheet("QLabel { padding: 0px 20px 0px 20px }");
  description->setWordWrap(true);

#ifndef Q_OS_WIN
  QFont small = font();
  small.setPointSize(small.pointSize() - 2);
  description->setFont(small);
#endif

  QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok, this);
  connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::close);
  connect(buttons, &QDialogButtonBox::rejected, this, &SettingsDialog::close);

  // Add edit button.
  QPushButton *edit = buttons->addButton(tr("Edit Config File..."),
                                         QDialogButtonBox::ResetRole);

  QWidget *widget = new QWidget(this);
  QVBoxLayout *layout = new QVBoxLayout(widget);
  layout->addWidget(stack);
  layout->addWidget(description);

#ifdef Q_OS_WIN
  layout->addSpacing(16);
#endif

  layout->addWidget(buttons);

  setCentralWidget(widget);

  // Track actions in a group.
  QActionGroup *actions = new QActionGroup(this);
  connect(actions, &QActionGroup::triggered, this,
          [this, stack, description, edit](QAction *action) {
            int index = action->data().toInt();
            bool config = (index < Window);
            description->setVisible(config);
            edit->setVisible(config);
            stack->setCurrentIndex(index);
            setWindowTitle(action->text());
          });

  // Add global git settings panel.
  QAction *general = toolbar->addAction(QIcon(":/general.png"), tr("General"));
  general->setData(General);
  general->setActionGroup(actions);
  general->setCheckable(true);

  stack->addWidget(new GeneralPanel(this));

  // Add diff panel.
  QAction *diff = toolbar->addAction(QIcon(":/diff.png"), tr("Diff"));
  diff->setData(Diff);
  diff->setActionGroup(actions);
  diff->setCheckable(true);

  stack->addWidget(new DiffPanel(git::Repository(), this));

  // Add tools panel.
  QAction *tools = toolbar->addAction(QIcon(":/tools.png"), tr("Tools"));
  tools->setData(Tools);
  tools->setActionGroup(actions);
  tools->setCheckable(true);

  stack->addWidget(new ToolsPanel(this));

  toolbar->addSeparator();

  // Add window panel.
  QAction *window = toolbar->addAction(QIcon(":/window.png"), tr("Window"));
  window->setData(Window);
  window->setActionGroup(actions);
  window->setCheckable(true);

  stack->addWidget(new WindowPanel(this));

  // Add editor panel.
  QAction *editor = toolbar->addAction(QIcon(":/editor.png"), tr("Editor"));
  editor->setData(Editor);
  editor->setActionGroup(actions);
  editor->setCheckable(true);

  stack->addWidget(new EditorPanel(this));

  // Add update panel.
  QAction *update = toolbar->addAction(QIcon(":/update.png"), tr("Update"));
  update->setData(Update);
  update->setActionGroup(actions);
  update->setCheckable(true);

  stack->addWidget(new UpdatePanel(this));

  // Add plugins panel.
  QAction *plugins = toolbar->addAction(QIcon(":/plugins.png"), tr("Plugins"));
  plugins->setData(Plugins);
  plugins->setActionGroup(actions);
  plugins->setCheckable(true);

  stack->addWidget(new PluginsPanel(git::Repository(), this));

  // Add misc panel.
  QAction *misc = toolbar->addAction(QIcon(":/misc.png"), tr("Misc"));
  misc->setData(Misc);
  misc->setActionGroup(actions);
  misc->setCheckable(true);

  stack->addWidget(new MiscPanel(this));

  // Add hotkeys panel.
  QAction *hotkeys = toolbar->addAction(QIcon(":/hotkeys.png"), tr("Hotkeys"));
  hotkeys->setData(Hotkeys);
  hotkeys->setActionGroup(actions);
  hotkeys->setCheckable(true);

  stack->addWidget(new HotkeysPanel(this));

#ifdef Q_OS_UNIX
  // Add terminal panel.
  QAction *terminal =
      toolbar->addAction(QIcon(":/terminal.png"), tr("Terminal"));
  terminal->setData(Terminal);
  terminal->setActionGroup(actions);
  terminal->setCheckable(true);

  stack->addWidget(new TerminalPanel(this));
#endif

  // Hook up edit button.
  connect(edit, &QPushButton::clicked, stack, [stack] {
    // Update on save.
    EditorWindow *window = EditorWindow::open(git::Config::globalPath());
    GeneralPanel *panel = static_cast<GeneralPanel *>(stack->widget(General));
    connect(window->widget(), &BlameEditor::saved, panel, &GeneralPanel::init);
  });

  // Select the requested index.
  actions->actions().at(index)->trigger();

  setMinimumWidth(toolbar->sizeHint().width());
}

void SettingsDialog::openSharedInstance(Index index) {
  static QPointer<SettingsDialog> dialog;
  if (dialog) {
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
    return;
  }

  dialog = new SettingsDialog(index);
  dialog->show();
}
