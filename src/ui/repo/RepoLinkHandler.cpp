//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "RepoLinkHandler.h"
#include "dialogs/RemoteDialog.h"
#include "dialogs/SettingsDialog.h"
#include "git/Config.h"
#include "ui/repo/RepoView.h"
#include <QDesktopServices>
#include <QHash>
#include <QMessageBox>
#include <QUrl>
#include <QUrlQuery>

namespace {

struct Link {
  QUrl url;
  QUrlQuery query;
  git::Reference ref;
};

using Handler = void (*)(RepoView *view, const Link &link);

bool boolParam(const QUrlQuery &query, const QString &key) {
  return query.queryItemValue(key) == "true";
}

RepoView::MergeFlags fastForwardFlags(const QUrlQuery &query) {
  return boolParam(query, "no-ff") ? RepoView::NoFastForward
                                   : RepoView::Default;
}

void disableSslVerify(git::Config config, const QString &text,
                      const QString &path) {
  config.setValue<bool>("http.sslVerify", false);
  QMessageBox msg(QMessageBox::Icon::Information,
                  RepoView::tr("Certificate Error"), text,
                  QMessageBox::Button::Ok);
  msg.setDetailedText(RepoView::tr("[http]\n"
                                   "  sslVerify = false\n\n"
                                   "was added to %1")
                          .arg(path));
  msg.exec();
}

const QHash<QString, Handler> &actionHandlers() {
  static const QHash<QString, Handler> handlers = {
      {"pull", [](RepoView *view, const Link &) { view->pull(); }},
      {"push",
       [](RepoView *view, const Link &link) {
         git::Remote remote;
         QString to = link.query.queryItemValue("to");
         if (!to.isEmpty())
           remote = view->repo().lookupRemote(to);

         if (boolParam(link.query, "force")) {
           view->promptToForcePush(remote, link.ref);
         } else {
           view->push(remote, link.ref, QString(),
                      boolParam(link.query, "set-upstream"));
         }
       }},
      {"push-to",
       [](RepoView *view, const Link &) {
         RemoteDialog *dialog = new RemoteDialog(RemoteDialog::Push, view);
         dialog->open();
       }},
      {"add-remote",
       [](RepoView *view, const Link &link) {
         ConfigDialog *dialog = view->configureSettings(ConfigDialog::Remotes);
         dialog->addRemote(link.query.queryItemValue("name"));
       }},
      {"stash", [](RepoView *view, const Link &) { view->promptToStash(); }},
      {"unstash", [](RepoView *view, const Link &) { view->popStash(); }},
      {"checkout",
       [](RepoView *view, const Link &link) {
         if (link.ref.isValid()) {
           view->checkout(link.ref, boolParam(link.query, "detach"));
         } else {
           view->promptToCheckout();
         }
       }},
      {"fast-forward",
       [](RepoView *view, const Link &link) {
         view->merge(RepoView::FastForward, link.ref);
       }},
      {"merge",
       [](RepoView *view, const Link &link) {
         view->merge(fastForwardFlags(link.query) | RepoView::Merge, link.ref);
       }},
      {"rebase",
       [](RepoView *view, const Link &link) {
         view->merge(fastForwardFlags(link.query) | RepoView::Rebase, link.ref);
       }},
      {"config",
       [](RepoView *view, const Link &link) {
         if (boolParam(link.query, "global")) {
           SettingsDialog::openSharedInstance();
         } else {
           view->configureSettings(ConfigDialog::General);
         }
       }},
      {"amend", [](RepoView *view, const Link &) { view->amendCommit(); }},
      {"abort", [](RepoView *view, const Link &) { view->promptToAbort(); }},
      {"sslverifyrepo",
       [](RepoView *view, const Link &) {
         git::Repository repo = view->repo();
         if (repo.isValid())
           disableSslVerify(
               repo.gitConfig(),
               RepoView::tr("SSL verification disabled for this repository"),
               repo.dir().filePath("config"));
       }},
      {"sslverifygit",
       [](RepoView *, const Link &) {
         git::Config config = git::Config::global();
         if (config.isValid())
           disableSslVerify(
               config,
               RepoView::tr("SSL verification disabled for all git "
                            "repositories"),
               config.globalPath());
       }},
  };

  return handlers;
}

const QHash<QString, Handler> &schemeHandlers() {
  static const QHash<QString, Handler> handlers = {
      {"http", [](RepoView *,
                  const Link &link) { QDesktopServices::openUrl(link.url); }},
      {"https", [](RepoView *,
                   const Link &link) { QDesktopServices::openUrl(link.url); }},
      {"id",
       [](RepoView *view, const Link &link) {
         if (link.ref.isValid())
           view->selectReference(link.ref);
         git::Commit commit = view->repo().lookupCommit(link.url.path());
         if (commit.isValid())
           view->selectCommit(commit, link.query.queryItemValue("file"));
       }},
      {"submodule",
       [](RepoView *view, const Link &link) {
         view->openSubmodule(view->repo().lookupSubmodule(link.url.path()));
       }},
      {"action",
       [](RepoView *view, const Link &link) {
         if (Handler handler = actionHandlers().value(link.url.path()))
           handler(view, link);
       }},
  };

  return handlers;
}

} // namespace

namespace RepoLinkHandler {

void visit(RepoView *view, const QString &link) {
  Link parsed;
  parsed.url = QUrl(link);
  parsed.query = QUrlQuery(parsed.url.query());

  QString refName = parsed.query.queryItemValue("ref");
  if (!refName.isEmpty())
    parsed.ref = view->repo().lookupRef(refName);

  if (Handler handler = schemeHandlers().value(parsed.url.scheme()))
    handler(view, parsed);
}

} // namespace RepoLinkHandler
