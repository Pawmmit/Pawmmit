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
#include "ui/window/MainWindow.h"

using namespace Test;
using namespace QTest;

class TestMainWindow : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void show();
  void cleanupTestCase();

private:
  ScratchRepository mRepo;
  MainWindow *mWindow = nullptr;
};

void TestMainWindow::initTestCase() { mWindow = new MainWindow(mRepo); }

void TestMainWindow::show() {
  mWindow->show();
  QVERIFY(qWaitForWindowActive(mWindow));
}

void TestMainWindow::cleanupTestCase() { mWindow->close(); }

TEST_MAIN(TestMainWindow)

#include "main_window.moc"
