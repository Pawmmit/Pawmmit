//
//          Copyright (c) 2016, Scientific Toolworks, Inc.
//
// This software is licensed under the GNU General Public License v3.0 or
// (at your option) any later version. The LICENSE.md file describes the
// conditions under which this software may be distributed.
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Author: Bryan Williams
//

#include "LfsPanel.h"
#include "git/Config.h"
#include "ui/Footer.h"
#include "ui/repo/RepoView.h"
#include "ui_LfsPanel.h"
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStringListModel>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QtConcurrent>

LfsPanel::LfsPanel(RepoView *view, QWidget *parent)
    : QWidget(parent), ui(new Ui::LfsPanel) {
  ui->setupUi(this);

  if (!view->repo().lfsIsInitialized()) {
    ui->mInitializedPage->setVisible(false);
    connect(ui->mInitializeButton, &QPushButton::clicked, this, [this, view] {
      view->lfsInitialize();
      window()->close();
    });
    return;
  }

  ui->mNotInitializedPage->setVisible(false);

  git::Repository repo = view->repo();

  QStringListModel *includedModel = new QStringListModel(QStringList(), this);
  ui->mIncludedList->setModel(includedModel);

  QStringListModel *excludedModel = new QStringListModel(QStringList(), this);
  ui->mExcludedList->setModel(excludedModel);

  QFutureWatcher<git::Repository::LfsTracking> *watcher =
      new QFutureWatcher<git::Repository::LfsTracking>(this);
  connect(watcher, &QFutureWatcher<QStringList>::finished, this,
          [includedModel, excludedModel, watcher] {
            includedModel->setStringList(watcher->result().included);
            excludedModel->setStringList(watcher->result().excluded);
            watcher->deleteLater();
          });

  watcher->setFuture(QtConcurrent::run(&git::Repository::lfsTracked, repo));

  connect(
      ui->mIncludedFooter, &Footer::plusClicked, this,
      [this, repo, includedModel, excludedModel] {
        QDialog *dialog = new QDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);

        QLabel *description = new QLabel(
            tr("Specify a glob pattern for tracking large files.\n"
               "\n"
               "Generally, large files are greater than 500kB, change "
               "frequently,\n"
               "and do not compress well with git. This includes binary "
               "or video\n"
               "files which are already highly compressed.\n"
               "\n"
               "Examples\n"
               "*.png\n"
               "*.[pP][nN][gG]\n"
               "/images/*\n"));

        QFormLayout *form = new QFormLayout;
        QLineEdit *pattern = new QLineEdit(dialog);
        form->addRow(tr("Pattern:"), pattern);

        QDialogButtonBox *buttons = new QDialogButtonBox();
        buttons->addButton(QDialogButtonBox::Cancel);
        QPushButton *track =
            buttons->addButton(tr("Track"), QDialogButtonBox::AcceptRole);
        track->setEnabled(false);
        connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);

        QVBoxLayout *layout = new QVBoxLayout(dialog);
        layout->addWidget(description);
        layout->addLayout(form);
        layout->addWidget(buttons);

        connect(pattern, &QLineEdit::textChanged, this,
                [track](const QString &text) {
                  track->setEnabled(!text.isEmpty());
                });

        connect(dialog, &QDialog::accepted, this,
                [pattern, repo, includedModel, excludedModel] {
                  git::Repository tmp(repo);
                  tmp.lfsSetTracked(pattern->text(), true);
                  auto tracking = tmp.lfsTracked();
                  includedModel->setStringList(tracking.included);
                  excludedModel->setStringList(tracking.excluded);
                });

        dialog->open();
      });

  connect(ui->mIncludedFooter, &Footer::minusClicked, this,
          [this, repo, includedModel, excludedModel] {
            git::Repository tmp(repo);
            QModelIndexList indexes =
                ui->mIncludedList->selectionModel()->selectedRows();
            for (const QModelIndex &index : indexes) {
              QString text = index.data(Qt::DisplayRole).toString();
              tmp.lfsSetTracked(text, false);
            }

            auto tracking = tmp.lfsTracked();
            includedModel->setStringList(tracking.included);
            excludedModel->setStringList(tracking.excluded);
          });

  // enable minus button
  auto updateMinusButton = [this] {
    ui->mIncludedFooter->setMinusEnabled(
        ui->mIncludedList->selectionModel()->hasSelection());
  };
  connect(ui->mIncludedList->selectionModel(),
          &QItemSelectionModel::selectionChanged, this, updateMinusButton);
  connect(ui->mIncludedList->model(), &QAbstractItemModel::modelReset, this,
          updateMinusButton);

  QMap<QString, QString> map;
  for (const QString &string : repo.lfsEnvironment()) {
    if (string.contains("=")) {
      QString key = string.section('=', 0, 0);
      QString value = string.section('=', 1);
      map.insert(key, value);
    }
  }

  ui->mUrl->setText(map.value("Endpoint").section(" ", 0, 0));
  connect(ui->mUrl, &QLineEdit::textChanged, [repo](const QString &text) {
    git::Config config = repo.gitConfig();
    config.setValue("lfs.url", text);
  });

  using Signal = void (QSpinBox::*)(int);
  auto signal = static_cast<Signal>(&QSpinBox::valueChanged);

  ui->mPruneOffsetDays->setValue(map.value("PruneOffsetDays").toInt());
  connect(ui->mPruneOffsetDays, signal, [repo](int value) {
    git::Config config = repo.gitConfig();
    config.setValue("lfs.pruneoffsetdays", value);
  });

  bool fetchRecentEnabled = map.value("FetchRecentAlways").contains("true");
  ui->mFetchRecentAlways->setChecked(fetchRecentEnabled);
  connect(ui->mFetchRecentAlways, &QCheckBox::toggled, [repo](bool checked) {
    git::Config config = repo.gitConfig();
    config.setValue("lfs.fetchrecentalways", checked);
  });

  ui->mFetchRecentRefsDays->setValue(map.value("FetchRecentRefsDays").toInt());
  ui->mFetchRecentRefsDays->setEnabled(fetchRecentEnabled);
  connect(ui->mFetchRecentRefsDays, signal, [repo](int value) {
    git::Config config = repo.gitConfig();
    config.setValue("lfs.fetchrecentrefsdays", value);
  });
  connect(
      ui->mFetchRecentAlways, &QCheckBox::toggled, this,
      [this](bool checked) { ui->mFetchRecentRefsDays->setEnabled(checked); });

  ui->mFetchRecentCommitsDays->setValue(
      map.value("FetchRecentCommitsDays").toInt());
  ui->mFetchRecentCommitsDays->setEnabled(fetchRecentEnabled);
  connect(ui->mFetchRecentCommitsDays, signal, [repo](int value) {
    git::Config config = repo.gitConfig();
    config.setValue("lfs.fetchrecentcommitsdays", value);
  });
  connect(ui->mFetchRecentAlways, &QCheckBox::toggled, this,
          [this](bool checked) {
            ui->mFetchRecentCommitsDays->setEnabled(checked);
          });

  connect(ui->mEnvironment, &QAbstractButton::clicked, this, [view] {
    git::Repository repo = view->repo();

    QDialog dialog;
    dialog.setWindowTitle(tr("git-lfs env (read only)"));

    QSize size(500, 500);
    dialog.setFixedSize(size);

    QTextEdit *textEdit = new QTextEdit(&dialog);
    textEdit->setFixedSize(size);
    textEdit->setReadOnly(true);

    for (const QString &string : repo.lfsEnvironment()) {
      textEdit->append(string);
    }

    dialog.exec();
  });

  connect(ui->mDeinit, &QAbstractButton::clicked, this, [this, view] {
    QString title = tr("Deinitialize LFS?");
    QString text =
        tr("Are you sure you want uninstall LFS from this repository?");

    QMessageBox msg(QMessageBox::Warning, title, text, QMessageBox::Cancel,
                    this);

    QPushButton *agree =
        msg.addButton(tr("Deinitialize"), QMessageBox::AcceptRole);
    msg.exec();

    if (msg.clickedButton() == agree)
      view->lfsDeinitialize();

    window()->close();
  });
}

LfsPanel::~LfsPanel() = default;
