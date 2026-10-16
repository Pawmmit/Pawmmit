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

#include "TabWidget.h"
#include "MenuBar.h"
#include "TabBar.h"
#include "app/Application.h"
#include "dialogs/AccountDialog.h"
#include "host/Account.h"
#include "ui/repo/RepoView.h"
#include "ui/window/MainWindow.h"
#include <QFileDialog>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QStyle>
#include <QVBoxLayout>

namespace {

const QString kLinkFmt = "<a href='%1'>%2</a>";
const QString kSupportLink = "https://github.com/Pawmmit/Pawmmit/discussions";

class DefaultWidget : public QFrame {
  Q_OBJECT

public:
  DefaultWidget(QWidget *parent = nullptr) : QFrame(parent) {
    setFrameShape(QFrame::Box);
    setAutoFillBackground(true);
    setBackgroundRole(QPalette::Base);

    QLabel *heading = new QLabel(tr("Get started"), this);
    heading->setAlignment(Qt::AlignHCenter);
    heading->setStyleSheet("color: palette(bright-text)");
    QFont headingFont = heading->font();
    headingFont.setBold(true);
    headingFont.setCapitalization(QFont::AllUppercase);
    headingFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.6);
    heading->setFont(headingFont);

    QPushButton *clone =
        addButton(QIcon(":/clone.png"), tr("Clone Repository"));
    connect(clone, &QPushButton::clicked,
            [this] { MainWindow::promptToClone(this); });

    QPushButton *open = addButton(QIcon(":/open.png"), tr("Open Repository"));
    connect(open, &QPushButton::clicked,
            [this] { MainWindow::promptToOpen(this); });

    QPushButton *init =
        addButton(QIcon(":/new.png"), tr("Initialize New Repository"));
    connect(init, &QPushButton::clicked,
            [this] { MainWindow::promptToInit(this); });

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->addWidget(heading);
    layout->addWidget(clone);
    layout->addWidget(open);
    layout->addWidget(init);
    layout->addWidget(addSeparator());

    for (int i = 0; i < Account::NUM_KINDS; ++i) {
      Account::Kind kind = static_cast<Account::Kind>(i);
      QString text = tr("Add %1 Account").arg(Account::name(kind));
      QPushButton *account =
          addButton(Account::icon(kind), text, QSize(20, 20), 1);
      connect(account, &QPushButton::clicked, [this, kind] {
        AccountDialog *dialog = new AccountDialog(nullptr, this);
        dialog->setKind(kind);
        dialog->open();
      });

      layout->addWidget(account);
    }

    layout->addWidget(addSeparator());
    layout->addWidget(addLink(tr("Contact us for support"), kSupportLink));
  }

private:
  QPushButton *addButton(const QIcon &icon, const QString &text,
                         const QSize &iconSize = QSize(26, 26),
                         int pointSizeDelta = 4) {
    QPushButton *button = new QPushButton(icon, text, this);
    button->setStyleSheet("color: palette(bright-text); text-align: left");
    button->setIconSize(iconSize);
    button->setFlat(true);

    QFont font = button->font();
    font.setPointSize(font.pointSize() + pointSizeDelta);
    button->setFont(font);

    return button;
  }

  QLabel *addLink(const QString &text, const QString &link = QString()) {
    QLabel *label = new QLabel(kLinkFmt.arg(link, text), this);
    label->setAlignment(Qt::AlignHCenter);
    label->setOpenExternalLinks(true);

    QFont font = label->font();
    font.setPointSize(font.pointSize() + 3);
    label->setFont(font);

    return label;
  }

  QFrame *addSeparator() {
    QFrame *separator = new QFrame(this);
    separator->setStyleSheet("border: 1px solid palette(dark)");
    separator->setFrameShape(QFrame::HLine);
    return separator;
  }
};

} // namespace

TabWidget::TabWidget(QWidget *parent) : QTabWidget(parent) {
  TabBar *bar = new TabBar(this);
  bar->setMovable(true);
  bar->setTabsClosable(true);
  setTabBar(bar);

  // Create default widget.
  mDefaultWidget = new DefaultWidget(this);

  // Handle tab close.
  connect(this, &TabWidget::tabCloseRequested, [this](int index) {
    emit tabAboutToBeRemoved();
    widget(index)->close();
  });
}

void TabWidget::resizeEvent(QResizeEvent *event) {
  QTabWidget::resizeEvent(event);

  QSize size = event->size();
  QSize sizeHint = mDefaultWidget->sizeHint();
  int x = (size.width() - sizeHint.width()) / 2;
  int y = (size.height() - sizeHint.height()) / 2;
  mDefaultWidget->move(x, y);
}

void TabWidget::tabInserted(int index) {
  QTabWidget::tabInserted(index);

  // Qt's close button has a tool tip but no accessible name.
  auto side = static_cast<QTabBar::ButtonPosition>(style()->styleHint(
      QStyle::SH_TabBar_CloseButtonPosition, nullptr, tabBar()));
  if (QWidget *close = tabBar()->tabButton(index, side))
    close->setAccessibleName(
        QCoreApplication::translate("QTabBar", "Close Tab"));

  MenuBar::instance(this)->updateWindow();
  emit tabInserted();

  mDefaultWidget->setVisible(false);
}

void TabWidget::tabRemoved(int index) {
  QTabWidget::tabRemoved(index);
  MenuBar::instance(this)->updateWindow();
  emit tabRemoved();

  mDefaultWidget->setVisible(!count());
}

#include "TabWidget.moc"
