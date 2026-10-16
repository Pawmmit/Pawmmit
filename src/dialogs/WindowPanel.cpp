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

#include "WindowPanel.h"
#include "app/CustomTheme.h"
#include "conf/Settings.h"
#include "ui/window/EditorWindow.h"
#include "ui_WindowPanel.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDirIterator>
#include <QFile>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTimer>

WindowPanel::WindowPanel(QWidget *parent)
    : QWidget(parent), ui(new Ui::WindowPanel) {
  ui->setupUi(this);

  Settings *settings = Settings::instance();

  QComboBox *comboBox = ui->mTheme;

  // default theme
  comboBox->addItem("Default");

  // predefined themes
  QDir dir = Settings::themesDir();
  dir.setNameFilters({"*.lua"});
  QDirIterator *it = new QDirIterator(dir);
  while (it->hasNext()) {
    it->next();
    QString name = it->fileInfo().baseName();
    if (name != "Default")
      comboBox->addItem(name);
  }

  // user themes
  bool exists = false;
  QDir appLocalDir = CustomTheme::userDir(false, &exists);
  if (exists) {
    appLocalDir.setNameFilters({"*.lua"});
    QDirIterator *it = new QDirIterator(appLocalDir);

    if (it->hasNext())
      comboBox->insertSeparator(comboBox->count());

    while (it->hasNext()) {
      it->next();
      comboBox->addItem(it->fileInfo().baseName(), it->filePath());
    }
  }

  comboBox->insertSeparator(comboBox->count());

  int index =
      comboBox->findText(settings->value(Setting::Id::ColorTheme).toString());

  // add theme
  comboBox->addItem(tr("Add New Theme"));
  comboBox->addItem(tr("Edit Current Theme"), index);

  // Select the current theme.
  comboBox->setCurrentIndex(index >= 0 ? index : 0);

  // Edit enabled for user themes
  QStandardItemModel *model =
      static_cast<QStandardItemModel *>(comboBox->model());
  if (!comboBox->itemData(comboBox->currentIndex()).isValid())
    model->item(comboBox->count() - 1)->setEnabled(false);

  auto signal = QOverload<int>::of(&QComboBox::currentIndexChanged);
  connect(comboBox, signal, this, [this, parent, comboBox] {
    // Add new theme
    if (comboBox->currentIndex() == comboBox->count() - 2) {
      QDialog dialog;

      QDialogButtonBox *buttons =
          new QDialogButtonBox(QDialogButtonBox::Cancel, &dialog);
      connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
      connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

      QPushButton *create =
          buttons->addButton(tr("Create Theme"), QDialogButtonBox::AcceptRole);
      create->setEnabled(false);

      QLineEdit *nameField = new QLineEdit(&dialog);
      connect(nameField, &QLineEdit::textChanged, this, [create, nameField] {
        create->setEnabled(!nameField->text().isEmpty());
      });

      QFormLayout *layout = new QFormLayout(&dialog);
      layout->addRow(tr("Theme Name"), nameField);
      layout->addRow(buttons);

      if (dialog.exec()) {
        QDir dir = CustomTheme::userDir(true);
        QString path = dir.filePath(QString("%1.lua").arg(nameField->text()));
        QFile::copy(Settings::themesDir().filePath("Dark.lua"), path);
        EditorWindow::open(path);
      }

      window()->close();
      return;
    }

    // Edit current theme
    QStandardItemModel *model =
        static_cast<QStandardItemModel *>(comboBox->model());
    bool enabled = comboBox->itemData(comboBox->currentIndex()).isValid();
    model->item(comboBox->count() - 1)->setEnabled(enabled);

    if (comboBox->currentIndex() == comboBox->count() - 1) {
      int index = comboBox->currentData().toInt();
      QString path = comboBox->itemData(index).toString();
      EditorWindow::open(path);
      parent->close();
      return;
    }

    // Save theme
    Settings::instance()->setValue(Setting::Id::ColorTheme,
                                   comboBox->currentText());

    QMessageBox mb(QMessageBox::Information, tr("Restart?"),
                   tr("The application must be restarted for "
                      "the theme change to take effect."));
    mb.setInformativeText(tr("Do you want to restart now?"));
    QPushButton *restart = mb.addButton(tr("Restart"), QMessageBox::AcceptRole);
    mb.addButton(tr("Later"), QMessageBox::RejectRole);
    mb.setDefaultButton(restart);
    mb.exec();

    if (mb.clickedButton() == restart) {
      QWidget *dialog = window();
      QTimer::singleShot(0, this, [dialog] {
        // Close the dialog.
        dialog->close();

        // Restart the app.
        QStringList args = qApp->arguments();
        QProcess::startDetached(args.takeFirst(), args);
        qApp->quit();
      });
    }
  });

  ui->mFullPath->setChecked(
      settings->value(Setting::Id::ShowFullRepoPath).toBool());
  connect(ui->mFullPath, &QCheckBox::toggled, [](bool checked) {
    Settings::instance()->setValue(Setting::Id::ShowFullRepoPath, checked);
  });

  ui->mHideLog->setChecked(
      settings->value(Setting::Id::HideLogAutomatically).toBool());
  connect(ui->mHideLog, &QCheckBox::toggled, [](bool checked) {
    Settings::instance()->setValue(Setting::Id::HideLogAutomatically, checked);
  });

  ui->mSmTabs->setChecked(
      settings->value(Setting::Id::OpenSubmodulesInTabs).toBool());
  connect(ui->mSmTabs, &QCheckBox::toggled, [](bool checked) {
    Settings::instance()->setValue(Setting::Id::OpenSubmodulesInTabs, checked);
  });

  ui->mRepoTabs->setChecked(
      settings->value(Setting::Id::OpenAllReposInTabs).toBool());
  connect(ui->mRepoTabs, &QCheckBox::toggled, [](bool checked) {
    Settings::instance()->setValue(Setting::Id::OpenAllReposInTabs, checked);
  });

  ui->mHideMenuBar->setChecked(
      settings->value(Setting::Id::HideMenuBar).toBool());
  connect(ui->mHideMenuBar, &QCheckBox::toggled, [](bool checked) {
    Settings::instance()->setValue(Setting::Id::HideMenuBar, checked);
  });

  ui->mAutohideSidebar->setChecked(
      settings->value(Setting::Id::AutoHideRepoSiderbar).toBool());
  connect(ui->mAutohideSidebar, &QCheckBox::toggled, [](bool checked) {
    Settings::instance()->setValue(Setting::Id::AutoHideRepoSiderbar, checked);
  });

  ui->mShowAvatars->setChecked(
      settings->value(Setting::Id::ShowAvatars).toBool());
  connect(ui->mShowAvatars, &QCheckBox::toggled, [](bool checked) {
    Settings::instance()->setValue(Setting::Id::ShowAvatars, checked);
  });

  ui->mShowMaximized->setChecked(
      settings->value(Setting::Id::ShowMaximized).toBool());
  connect(ui->mShowMaximized, &QCheckBox::toggled, [](bool checked) {
    Settings::instance()->setValue(Setting::Id::ShowMaximized, checked);
  });

  ui->mMerge->setText(settings->promptDescription(Prompt::Kind::Merge));
  ui->mMerge->setChecked(settings->prompt(Prompt::Kind::Merge));
  connect(ui->mMerge, &QCheckBox::toggled, [](bool checked) {
    Settings::instance()->setPrompt(Prompt::Kind::Merge, checked);
  });

  ui->mRevert->setText(settings->promptDescription(Prompt::Kind::Revert));
  ui->mRevert->setChecked(settings->prompt(Prompt::Kind::Revert));
  connect(ui->mRevert, &QCheckBox::toggled, [](bool checked) {
    Settings::instance()->setPrompt(Prompt::Kind::Revert, checked);
  });

  ui->mCherryPick->setText(
      settings->promptDescription(Prompt::Kind::CherryPick));
  ui->mCherryPick->setChecked(settings->prompt(Prompt::Kind::CherryPick));
  connect(ui->mCherryPick, &QCheckBox::toggled, [](bool checked) {
    Settings::instance()->setPrompt(Prompt::Kind::CherryPick, checked);
  });

  ui->mStash->setText(settings->promptDescription(Prompt::Kind::Stash));
  ui->mStash->setChecked(settings->prompt(Prompt::Kind::Stash));
  connect(ui->mStash, &QCheckBox::toggled, [](bool checked) {
    Settings::instance()->setPrompt(Prompt::Kind::Stash, checked);
  });

  ui->mLargeFiles->setText(
      settings->promptDescription(Prompt::Kind::LargeFiles));
  ui->mLargeFiles->setChecked(settings->prompt(Prompt::Kind::LargeFiles));
  connect(ui->mLargeFiles, &QCheckBox::toggled, [](bool checked) {
    Settings::instance()->setPrompt(Prompt::Kind::LargeFiles, checked);
  });

  ui->mDirectories->setText(
      settings->promptDescription(Prompt::Kind::Directories));
  ui->mDirectories->setChecked(settings->prompt(Prompt::Kind::Directories));
  connect(ui->mDirectories, &QCheckBox::toggled, [](bool checked) {
    Settings::instance()->setPrompt(Prompt::Kind::Directories, checked);
  });
}

WindowPanel::~WindowPanel() = default;
