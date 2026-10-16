//
//          Copyright (c) 2021, Pawmmit
//
// This software is licensed under the GNU General Public License v3.0 or
// (at your option) any later version. The LICENSE.md file describes the
// conditions under which this software may be distributed.
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Author: Kas (https://github.com/exactly-one-kas)
//

#ifndef HOTKEYMANAGER_H
#define HOTKEYMANAGER_H

#include "conf/Settings.h"
#include <QAction>
#include <QKeySequence>
#include <QObject>
#include <QShortcut>
#include <QString>
#include <QVector>
#include <functional>

class HotkeyManager;
struct HotkeyManagerHandle;

class HotkeyHandle : public QObject {
public:
  HotkeyHandle(QMetaObject::Connection &&changeSignalConnection);
  ~HotkeyHandle();

private:
  QMetaObject::Connection changeSignalConnection;
};

class Hotkey {
  friend class HotkeyManager;

public:
  inline Hotkey() : mHandle(nullptr) {}

  HotkeyHandle *use(std::function<void(const QKeySequence &)> changeHandler,
                    HotkeyManager *manager = nullptr) const;
  HotkeyHandle *use(QAction *action, HotkeyManager *manager = nullptr) const;
  HotkeyHandle *use(QShortcut *shortcut,
                    HotkeyManager *manager = nullptr) const;

  QString label() const;
  QKeySequence currentKeys(const HotkeyManager *manager = nullptr) const;
  QString defaultKeys() const;
  void setKeys(const QKeySequence &keys, HotkeyManager *manager = nullptr);

  inline bool isValid() const { return mHandle != nullptr; }

  inline bool operator==(const Hotkey &op) const {
    return mHandle == op.mHandle;
  }

  inline bool operator!=(const Hotkey &op) const {
    return mHandle != op.mHandle;
  }

private:
  const HotkeyManagerHandle *mHandle;

  Hotkey(const HotkeyManagerHandle *handle);
};

Q_DECLARE_METATYPE(Hotkey)

class HotkeyManager : public QObject {
  friend class Hotkey;

  Q_OBJECT

public:
  static Hotkey registerHotkey(const char *defaultKeys, const char *configPath,
                               const char *label);
  static Hotkey registerHotkey(QKeySequence::StandardKey defaultKeys,
                               const char *configPath, const char *label);
  static HotkeyManager *instance();

  HotkeyManager(Settings *settings = nullptr);
  QVector<Hotkey> knownHotkeys() const;

signals:
  void changed(const HotkeyManagerHandle *handle);

private:
  static Hotkey registerHotkey(const char *defaultKeys,
                               QKeySequence::StandardKey standardKey,
                               const char *configPath, const char *label);

  Settings *mSettings;
  QVector<HotkeyManagerHandle *> mHandles;
  QVector<QKeySequence> mKeys;

  QKeySequence keys(const HotkeyManagerHandle *handle) const;
  void setKeys(const HotkeyManagerHandle *handle, const QKeySequence &keys);
};

#endif
