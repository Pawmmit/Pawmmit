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

#include "Test.h"
#include "conf/Settings.h"
#include "git/Config.h"

using namespace QTest;

class TestConfig : public QObject {
  Q_OBJECT

private slots:
  void mergetools();
};

void TestConfig::mergetools() {
  git::Config config = git::Config::global();

  // Add mergetools config file for default external merge tools.
  QString path = Settings::confDir().filePath("mergetools");
  config.addFile(QDir::toNativeSeparators(path), git::Config::kDefaultsLevel);

  // Iterate over difftool cmd entries.
  QSet<QString> names;
  git::Config::Iterator it = config.glob("difftool\\..*\\.cmd");
  while (git::Config::Entry entry = it.next()) {
    names.insert(entry.name());
    qDebug() << entry.name() << entry.value<QString>();
  }

  QVERIFY(names.contains("difftool.araxis.cmd"));
}

TEST_MAIN(TestConfig)

#include "config.moc"
