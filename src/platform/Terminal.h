//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#ifndef PLATFORM_TERMINAL_H
#define PLATFORM_TERMINAL_H

#include <QString>

namespace platform {

/// @brief Get the platform's default terminal command
/// @return Command if found, else empty string
QString defaultTerminalCommand();

/// @brief Start the terminal command in dir
/// @param command Which command to run
/// @param dir Work directory
/// @return Whether it could be started
bool openTerminal(const QString &command, const QString &dir);

} // namespace platform

#endif
