//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "UserInfo.h"

namespace platform {

QString userLoginName() { return qEnvironmentVariable("USERNAME"); }

QString userFullName() { return userLoginName(); }

} // namespace platform
