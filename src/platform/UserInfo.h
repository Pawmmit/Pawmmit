//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#ifndef PLATFORM_USERINFO_H
#define PLATFORM_USERINFO_H

#include <QString>

namespace platform {

/// @brief Get the current user's login name
/// @return Login name, or empty string if unknown
QString userLoginName();

/// @brief Get the current user's full name
/// @return Full name, falling back to the login name
QString userFullName();

} // namespace platform

#endif
