//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#ifndef PLATFORM_GITINSTALL_H
#define PLATFORM_GITINSTALL_H

#include <QString>

namespace platform {

/// @brief Get the root directory of the Git for Windows installation
/// @return Directory if found, else empty string (always empty elsewhere)
QString gitInstallDir();

} // namespace platform

#endif
