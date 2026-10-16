//
//          Copyright (c) 2017, Scientific Toolworks, Inc.
//
// This software is licensed under the GNU General Public License v3.0 or
// (at your option) any later version. The LICENSE.md file describes the
// conditions under which this software may be distributed.
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Author: Jason Haslam
//

#include "DiffPanel.h"
#include "ConfigDialog.h"
#include "conf/Settings.h"
#include "git/Config.h"
#include "ui/repo/RepoView.h"
#include "ui/window/MainWindow.h"
#include "ui_DiffPanel.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QSpinBox>

DiffPanel::DiffPanel(const git::Repository &repo, QWidget *parent)
    : QWidget(parent), mConfig(repo ? repo.gitConfig() : git::Config::global()),
      ui(new Ui::DiffPanel) {
  ui->setupUi(this);

  // diff context
  ui->mContext->setValue(mConfig.value<int>("diff.context", 3));

  auto contextSignal = QOverload<int>::of(&QSpinBox::valueChanged);
  connect(ui->mContext, contextSignal, [this](int value) {
    mConfig.setValue("diff.context", value);
    for (MainWindow *window : MainWindow::windows()) {
      for (int i = 0; i < window->count(); ++i)
        window->view(i)->refresh();
    }
  });

  // encoding

  static std::array encodings{
      "Utf8",  "Utf16",   "Utf16LE", "Utf16BE",
      "Utf32", "Utf32LE", "Utf32BE", "Latin1",
  };

  ui->mEncoding->addItem(tr("System Locale"), -1);
  ui->mEncoding->insertSeparator(ui->mEncoding->count());
  for (int i = 0; i < static_cast<int>(encodings.size()); i++) {
    ui->mEncoding->addItem(encodings[i], i);
  }
  QString name = mConfig.value<QString>("gui.encoding");
  if (!name.isEmpty())
    ui->mEncoding->setCurrentIndex(ui->mEncoding->findText(name));

  auto encodingSignal = QOverload<int>::of(&QComboBox::currentIndexChanged);
  connect(ui->mEncoding, encodingSignal, [this](int index) {
    if (ui->mEncoding->itemData(index).toInt() < 0) {
      mConfig.remove("gui.encoding");
    } else {
      mConfig.setValue("gui.encoding", ui->mEncoding->itemText(index));
    }

    for (MainWindow *window : MainWindow::windows()) {
      for (int i = 0; i < window->count(); ++i)
        window->view(i)->refresh();
    }
  });

  // Wrap lines
  ui->mWrapLines->setChecked(Settings::instance()->isTextEditorWrapLines());
  connect(ui->mWrapLines, &QCheckBox::toggled, [](bool wrap) {
    Settings::instance()->setTextEditorWrapLines(wrap);
  });

  // Remaining settings are strictly global.
  bool global = !qobject_cast<ConfigDialog *>(parent);
  ui->formLayout->setRowVisible(ui->mIgnoreWs, global);
  ui->formLayout->setRowVisible(ui->mCollapseAdded, global);
  ui->formLayout->setRowVisible(ui->mCollapseDeleted, global);
  if (!global)
    return;

  // ignore whitespace
  // The ignore whitespace option is global because it's
  // not a config setting. It's a flag (-w) to git diff.
  ui->mIgnoreWs->setChecked(Settings::instance()->isWhitespaceIgnored());
  connect(ui->mIgnoreWs, &QCheckBox::toggled, [](bool checked) {
    Settings::instance()->setWhitespaceIgnored(checked);
  });

  // auto collapse
  Settings *settings = Settings::instance();
  ui->mCollapseAdded->setChecked(
      settings->value(Setting::Id::AutoCollapseAddedFiles).toBool());
  connect(ui->mCollapseAdded, &QCheckBox::toggled, [settings](bool checked) {
    settings->setValue(Setting::Id::AutoCollapseAddedFiles, checked);
  });

  ui->mCollapseDeleted->setChecked(
      settings->value(Setting::Id::AutoCollapseDeletedFiles).toBool());
  connect(ui->mCollapseDeleted, &QCheckBox::toggled, [settings](bool checked) {
    settings->setValue(Setting::Id::AutoCollapseDeletedFiles, checked);
  });
}

DiffPanel::~DiffPanel() = default;
