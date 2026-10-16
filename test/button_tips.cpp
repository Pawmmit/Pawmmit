#include "Test.h"
#include "ui/detail/CommitEditor.h"
#include "ui/detail/DoubleTreeWidget.h"
#include "ui/filetree/TreeView.h"
#include "ui/repo/RepoView.h"
#include "ui/search/SearchField.h"
#include "ui/window/MainWindow.h"
#include "ui/window/MenuBar.h"
#include "ui/window/ToolBar.h"
#include <QAbstractButton>
#include <QCalendarWidget>
#include <QCoreApplication>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QTextEdit>

using namespace Test;
using namespace QTest;

class TestButtonTips : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void everyControlHasAName();
  void tipsShowTheDefaultHotkey();
  void tipsFollowARebinding();
  void logTipFollowsVisibility();
  void everyIconOnlyButtonHasAName();
  void qtsOwnButtonsAreLeftAlone();
  void commitEditorTipsShowHotkeys();
  void commitTipExplainsWhatIsMissing();
  void cleanupTestCase();

private:
  QAbstractButton *button(const QString &name) const {
    for (QAbstractButton *button :
         mWindow->toolBar()->findChildren<QAbstractButton *>()) {
      if (button->accessibleName() == name)
        return button;
    }

    return nullptr;
  }

  // Any button in the window, wherever it lives.
  QAbstractButton *anyButton(const QString &text) const {
    for (QAbstractButton *button : mWindow->findChildren<QAbstractButton *>()) {
      if (button->text() == text || button->accessibleName() == text)
        return button;
    }

    return nullptr;
  }

  static QString keys(const QString &portable) {
    return QKeySequence(portable).toString(QKeySequence::NativeText);
  }

  // Keep the test away from the user's real settings.
  QTemporaryDir mDir;
  ScratchRepository mRepo;
  MainWindow *mWindow = nullptr;
};

void TestButtonTips::initTestCase() {
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, mDir.path());

  mWindow = new MainWindow(mRepo);
  mWindow->show();
  QVERIFY(qWaitForWindowExposed(mWindow));
}

void TestButtonTips::everyControlHasAName() {
  int count = 0;
  for (QAbstractButton *button :
       mWindow->toolBar()->findChildren<QAbstractButton *>()) {
    // Qt's own overflow button isn't ours to name.
    if (button->objectName().startsWith("qt_"))
      continue;

    ++count;
    QVERIFY2(!button->accessibleName().isEmpty(),
             qPrintable(QString("%1 '%2' has no accessible name")
                            .arg(button->metaObject()->className(),
                                 button->toolTip())));
  }

  QVERIFY(count > 10);
  QVERIFY(!mWindow->toolBar()->searchField()->accessibleName().isEmpty());
}

void TestButtonTips::tipsShowTheDefaultHotkey() {
  QAbstractButton *fetch = button(ToolBar::tr("Fetch"));
  QVERIFY(fetch);
  QCOMPARE(fetch->toolTip(), QString("%1 (%2)")
                                 .arg(ToolBar::tr("Fetch"))
                                 .arg(keys("Ctrl+Shift+Alt+F")));

  // Nothing is bound to the terminal by default.
  QAbstractButton *terminal = button(ToolBar::tr("Open Terminal"));
  QVERIFY(terminal);
  QCOMPARE(terminal->toolTip(), ToolBar::tr("Open Terminal"));
}

void TestButtonTips::tipsFollowARebinding() {
  QAbstractButton *fetch = button(ToolBar::tr("Fetch"));
  QVERIFY(fetch);

  Hotkeys::fetch.setKeys(QKeySequence("Ctrl+Alt+Y"));
  QCOMPARE(
      fetch->toolTip(),
      QString("%1 (%2)").arg(ToolBar::tr("Fetch")).arg(keys("Ctrl+Alt+Y")));

  Hotkeys::fetch.setKeys(QKeySequence());
  QCOMPARE(fetch->toolTip(), ToolBar::tr("Fetch"));

  // The name is what a screen reader announces, so it never carries a hotkey.
  QCOMPARE(fetch->accessibleName(), ToolBar::tr("Fetch"));
}

void TestButtonTips::logTipFollowsVisibility() {
  RepoView *view = mWindow->currentView();
  QVERIFY(view);

  view->setLogVisible(false);
  QAbstractButton *show = button(ToolBar::tr("Show Log"));
  QVERIFY(show);

  view->setLogVisible(true);
  QAbstractButton *hide = button(ToolBar::tr("Hide Log"));
  QVERIFY(hide);
  QVERIFY(hide->toolTip().startsWith(ToolBar::tr("Hide Log")));
}

// Qt's own buttons, such as the line edit icons and the calendar popup's
// corner, aren't ours to name. Which of them exist, and what they are called,
// depends on the Qt version, so go by what they are rather than by name.
static bool isQtInternal(QAbstractButton *button) {
  QString type = button->metaObject()->className();
  if (type == "QLineEditIconButton" || type == "QTableCornerButton" ||
      button->objectName().startsWith("qt_")) {
    return true;
  }

  for (QWidget *widget = button->parentWidget(); widget;
       widget = widget->parentWidget()) {
    QString parent = widget->metaObject()->className();
    if (parent.startsWith("QtPrivate::") || parent == "QCalendarWidget")
      return true;
  }

  return false;
}

void TestButtonTips::everyIconOnlyButtonHasAName() {
  int checked = 0;
  for (QAbstractButton *button : mWindow->findChildren<QAbstractButton *>()) {
    if (isQtInternal(button) || !button->text().isEmpty())
      continue;

    ++checked;
    QString parent = button->parentWidget()
                         ? button->parentWidget()->metaObject()->className()
                         : "?";
    QVERIFY2(!button->accessibleName().isEmpty(),
             qPrintable(QString("%1 in %2 has no accessible name")
                            .arg(button->metaObject()->className(), parent)));
  }

  QVERIFY(checked > 15);
}

void TestButtonTips::qtsOwnButtonsAreLeftAlone() {
  // Older Qt versions, such as CI's, don't give the calendar's buttons object
  // names, so strip them to look like that.
  QCalendarWidget *calendar = new QCalendarWidget(mWindow);
  calendar->show();
  QVERIFY(qWaitForWindowExposed(mWindow));
  for (QAbstractButton *button : calendar->findChildren<QAbstractButton *>())
    button->setObjectName(QString());

  everyIconOnlyButtonHasAName();
  delete calendar;
}

void TestButtonTips::commitEditorTipsShowHotkeys() {
  QAbstractButton *stage = anyButton(CommitEditor::tr("Stage All"));
  QVERIFY(stage);
  QCOMPARE(stage->toolTip(), QString("%1 (%2)")
                                 .arg(CommitEditor::tr("Stage All"))
                                 .arg(keys("Ctrl++")));

  QAbstractButton *unstage = anyButton(CommitEditor::tr("Unstage All"));
  QVERIFY(unstage);
  QCOMPARE(unstage->toolTip(), QString("%1 (%2)")
                                   .arg(CommitEditor::tr("Unstage All"))
                                   .arg(keys("Ctrl+-")));

  QAbstractButton *commit = anyButton(CommitEditor::tr("Commit"));
  QVERIFY(commit);
  QVERIFY(commit->toolTip().startsWith(QString("%1 (%2)")
                                           .arg(CommitEditor::tr("Commit"))
                                           .arg(keys("Ctrl+Shift+C"))));

  QAbstractButton *copy =
      anyButton(QCoreApplication::translate("CommitDetail", "Copy Commit ID"));
  QVERIFY(copy);
}

void TestButtonTips::commitTipExplainsWhatIsMissing() {
  RepoView *view = mWindow->currentView();
  QVERIFY(view);

  QFile file(mRepo->workdir().filePath("test"));
  QVERIFY(file.open(QFile::WriteOnly));
  file.write("This will be a test.\n");
  file.close();
  refresh(view);

  auto doubleTree = view->findChild<DoubleTreeWidget *>();
  QVERIFY(doubleTree);
  auto files = doubleTree->findChild<TreeView *>("Unstaged");
  QVERIFY(files);
  QTRY_COMPARE_WITH_TIMEOUT(files->model()->rowCount(), 1, 10000);

  QAbstractButton *commit = anyButton(CommitEditor::tr("Commit"));
  QVERIFY(commit);
  QString base = QString("%1 (%2)")
                     .arg(CommitEditor::tr("Commit"))
                     .arg(keys("Ctrl+Shift+C"));

  QTRY_COMPARE(commit->toolTip(),
               base + "\n" +
                   CommitEditor::tr("Stage the files you want to commit") +
                   "\n" + CommitEditor::tr("Enter a commit message"));
  QVERIFY(!commit->isEnabled());

  // Staging a file suggests a message, so there is nothing left to say.
  QModelIndex index = files->model()->index(0, 0);
  mouseClick(files->viewport(), Qt::LeftButton, Qt::KeyboardModifiers(),
             files->checkRect(index).center());
  QTRY_COMPARE(commit->toolTip(), base);
  QVERIFY(commit->isEnabled());

  QTextEdit *editor = view->findChild<QTextEdit *>("MessageEditor");
  QVERIFY(editor);
  editor->clear();
  QTRY_COMPARE(commit->toolTip(),
               base + "\n" + CommitEditor::tr("Enter a commit message"));
  QVERIFY(!commit->isEnabled());

  editor->setText("base commit");
  QTRY_COMPARE(commit->toolTip(), base);
  QVERIFY(commit->isEnabled());
}

void TestButtonTips::cleanupTestCase() { mWindow->close(); }

TEST_MAIN(TestButtonTips)

#include "button_tips.moc"
