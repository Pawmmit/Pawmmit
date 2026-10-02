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

#include "UpdateDialog.h"
#include "DownloadDialog.h"
#include "Updater.h"
#include "conf/Settings.h"
#include "dialogs/IconLabel.h"
#include "ui/window/MenuBar.h"
#include <QCheckBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

#if ((!defined(Q_OS_LINUX) || defined(FLATPAK) || defined(DEBUG_FLATPAK)) &&   \
     defined(ENABLE_UPDATE_OVER_GUI))
#define ENABLE_UPDATE 1
#else
#define ENABLE_UPDATE 0
#endif
namespace {

const QString kStyleSheet = "h3 {"
                            "  color: #696969;"
                            "  text-decoration: underline"
                            "}";

} // namespace

UpdateDialog::UpdateDialog(const QString &platform, const QString &version,
                           const QString &changelog, const QString &link,
                           QWidget *parent)
    : QDialog(parent) {
  QString appName = QCoreApplication::applicationName();
  QString appVersion = QCoreApplication::applicationVersion();

  setAttribute(Qt::WA_DeleteOnClose);
  setWindowTitle(tr("Update %1").arg(appName));

  QIcon icon(":/Pawmmit.iconset/icon_128x128.png");
  IconLabel *iconLabel = new IconLabel(icon, 128, 128, this);

  QVBoxLayout *iconLayout = new QVBoxLayout;
  iconLayout->addWidget(iconLabel);
  iconLayout->addStretch();

#if !ENABLE_UPDATE
  QString label = tr("<h3>A new version of %1 is available!</h3>"
                     "<p>%1 %2 is now available - you have %3. "
                     "The new version will be soon available in your package "
                     "manager. Just update your system.</p>"
                     "<b>Release Notes:</b>")
                      .arg(appName, version, appVersion);
#elif !defined(Q_OS_LINUX)
  QString label = tr("<h3>A new version of %1 is available!</h3>"
                     "<p>%1 %2 is now available - you have %3. "
                     "Would you like to download it now?</p>"
                     "<b>Release Notes:</b>")
                      .arg(appName, version, appVersion);
#elif defined(FLATPAK) || defined(DEBUG_FLATPAK)
  QString label =
      tr("<h3>A new version of %1 is available!</h3>"
         "<p>%1 %2 is now available - you have %3.</p>"
         "<p>If you downloaded the flatpak package over a package manager or "
         "from flathub.org <br/>"
         "you don't have to install manually a new version. It will be "
         "available within the next <br/>"
         "days during your system update: <code>flatpak update</code></p>"
         "<b>Release Notes:</b>")
          .arg(appName, version, appVersion);
#else
  QString label = tr("<h3>A new version of %1 is available!</h3>"
                     "<p>%1 %2 is now available - you have %3. "
                     "The new version will be soon available in your package "
                     "manager. Just update your system.</p>"
                     "<b>Release Notes:</b>")
                      .arg(appName, version, appVersion);
#endif

  QTextBrowser *browser = new QTextBrowser;
  browser->document()->setDocumentMargin(12);
  browser->document()->setDefaultStyleSheet(kStyleSheet);
  browser->setHtml(changelog);

#if ENABLE_UPDATE
  QCheckBox *download =
      new QCheckBox(tr("Automatically download and install updates"), this);
  download->setChecked(Settings::instance()
                           ->value(Setting::Id::InstallUpdatesAutomatically)
                           .toBool());
  connect(download, &QCheckBox::toggled, [](bool checked) {
    Settings::instance()->setValue(Setting::Id::InstallUpdatesAutomatically,
                                   checked);
  });
#endif

  QDialogButtonBox *buttons = new QDialogButtonBox(this);
#if ENABLE_UPDATE
  buttons->addButton(tr("Install Update"), QDialogButtonBox::AcceptRole);

  buttons->addButton(tr("Remind Me Later"), QDialogButtonBox::RejectRole);
  connect(buttons, &QDialogButtonBox::accepted, this, &UpdateDialog::accept);

  QPushButton *skip =
      buttons->addButton(tr("Skip This Version"), QDialogButtonBox::ResetRole);

  connect(skip, &QPushButton::clicked, [this, version] {
    Settings *settings = Settings::instance();
    QStringList skipped =
        settings->value(Setting::Id::SkippedUpdates).toStringList();
    if (!skipped.contains(version))
      settings->setValue(Setting::Id::SkippedUpdates, skipped << version);
    reject();
  });

  connect(buttons, &QDialogButtonBox::rejected, this, &UpdateDialog::reject);
  connect(this, &UpdateDialog::accepted, [link] {
    // Start download.
    if (Updater::DownloadRef download = Updater::instance()->download(link)) {
      DownloadDialog *dialog = new DownloadDialog(download);
      dialog->show();
    }
  });
#else
  buttons->addButton(tr("Ok"), QDialogButtonBox::AcceptRole);
  connect(buttons, &QDialogButtonBox::accepted, this, &UpdateDialog::accept);
  // Skip version automatically, because the user has no control to update
  Settings *settings = Settings::instance();
  QStringList skipped =
      settings->value(Setting::Id::SkippedUpdates).toStringList();
  if (!skipped.contains(version))
    settings->setValue(Setting::Id::SkippedUpdates, skipped << version);
#endif // ENABLE_UPDATE

  QHBoxLayout *l = new QHBoxLayout();
  QSpacerItem *spacer =
      new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
#if ENABLE_UPDATE
  l->addWidget(download);
#endif // ENABLE_UPDATE
  l->addItem(spacer);

  QVBoxLayout *content = new QVBoxLayout;
  content->addWidget(new QLabel(label, this));
  content->addWidget(browser);
  content->addLayout(l);
  content->addWidget(buttons);

  QHBoxLayout *layout = new QHBoxLayout(this);
  layout->addLayout(iconLayout);
  layout->addLayout(content);
}
