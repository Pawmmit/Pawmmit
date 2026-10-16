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

#include "CloneDialog.h"
#include "LocationPage.h"
#include "RemotePage.h"
#include "git/Remote.h"
#include "git/Repository.h"
#include "git/Result.h"
#include "log/LogEntry.h"
#include "log/LogView.h"
#include "ui/repo/RemoteCallbacks.h"
#include <QVBoxLayout>
#include <QtConcurrent>

namespace {

const QString kPathKey = "repo/path";

class ClonePage : public QWizardPage {
  Q_OBJECT

public:
  ClonePage(QWidget *parent = nullptr) : QWizardPage(parent) {
    setTitle(tr("Clone Progress"));
    setSubTitle(tr("The new repository will open after the clone finishes."));

    mLogRoot = new LogEntry(this);
    mLogView = new LogView(mLogRoot, this);
    connect(mLogView, &LogView::operationCanceled, this, &ClonePage::cancel);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(mLogView);
  }

  bool isComplete() const override {
    return (!mWatcher || mWatcher->isFinished());
  }

  void initializePage() override {
    QString url = field("url").toString().trimmed();
    QString name = field("name").toString().trimmed();
    QString path = QDir(field("path").toString().trimmed()).filePath(name);
    bool bare = field("bare").toBool();
    LogEntry *entry = mLogRoot->addEntry(url, tr("Clone"));

    mWatcher = new QFutureWatcher<git::Result>(this);
    connect(mWatcher, &QFutureWatcher<git::Result>::finished, mWatcher,
            [this, path, entry] {
              entry->setBusy(false);

              git::Result result = mWatcher->result();
              if (mCallbacks->isCanceled()) {
                error(entry, tr("clone"), path, tr("Clone canceled."));
              } else if (!result) {
                error(entry, tr("clone"), path, result.errorString());
              } else {
                mCallbacks->storeDeferredCredentials();
                emit completeChanged();
              }

              mWatcher->deleteLater();

              mWatcher = nullptr;
              mCallbacks = nullptr;
            });

    mCallbacks = new RemoteCallbacks(RemoteCallbacks::Receive, entry, url,
                                     "origin", mWatcher);

    entry->setBusy(true);
    mWatcher->setFuture(
        QtConcurrent::run(&git::Remote::clone, mCallbacks, url, path, bare));
  }

  void cleanupPage() override { cancel(); }

private:
  void cancel() {
    // Signal the asynchronous transfer to cancel itself.
    // Wait for it to finish before leaving this page.
    if (mWatcher && mWatcher->isRunning()) {
      mCallbacks->setCanceled(true);
      mWatcher->waitForFinished();
    }
  }

  void error(LogEntry *entry, const QString &action, const QString &name,
             const QString &defaultError) {
    QString text = tr("Failed to %1 into '%2' - %3");
    QString detail = git::Repository::lastError(defaultError);
    entry->addEntry(LogEntry::Error, text.arg(action, name, detail));
  }

  LogEntry *mLogRoot;
  LogView *mLogView;

  RemoteCallbacks *mCallbacks = nullptr;
  QFutureWatcher<git::Result> *mWatcher = nullptr;
};

} // namespace

CloneDialog::CloneDialog(Kind kind, QWidget *parent, Repository *repo)
    : QWizard(parent) {
  bool init = (kind == Init);
  setAttribute(Qt::WA_DeleteOnClose);
  setWindowTitle(init ? tr("Initialize Repository") : tr("Clone Repository"));
  setOptions(QWizard::NoBackButtonOnStartPage | QWizard::CancelButtonOnLeft);
  setWizardStyle(QWizard::ModernStyle);

  addPage(new RemotePage(repo, this));
  int location = addPage(new LocationPage(init, this));
  int clone = addPage(new ClonePage(this));

  connect(page(clone), &ClonePage::completeChanged, [this, clone] {
    if (page(clone)->isComplete())
      accept();
  });

  if (init)
    setStartId(location);
}

void CloneDialog::accept() {
  QString path = this->path();
  bool bare = field("bare").toBool();
  if (git::Repository::open(path).isValid() ||
      git::Repository::init(path, bare).isValid()) {
    QSettings().setValue(kPathKey, field("path"));
    QDialog::accept();
  }

  // FIXME: Report error.
}

QString CloneDialog::path() const {
  return QDir(field("path").toString()).filePath(field("name").toString());
}

QString CloneDialog::message() const {
  QString url = field("url").toString();
  return url.isEmpty()
             ? tr("Initialized empty repository into '%1'").arg(path())
             : tr("Cloned repository from '%1' into '%2'").arg(url, path());
}

QString CloneDialog::messageTitle() const {
  QString url = field("url").toString();
  return url.isEmpty() ? tr("Initialize") : tr("Clone");
}

#include "CloneDialog.moc"
