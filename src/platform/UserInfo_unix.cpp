//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "UserInfo.h"
#include <pwd.h>
#include <unistd.h>

namespace platform {

QString userLoginName() {
  passwd *pw = getpwuid(getuid());
  if (pw && pw->pw_name && *pw->pw_name)
    return QString::fromLocal8Bit(pw->pw_name);

  return qEnvironmentVariable("USER");
}

QString userFullName() {
  QString login = userLoginName();

  // The full name is the first comma-separated field of GECOS.
  QString name;
  passwd *pw = getpwuid(getuid());
  if (pw && pw->pw_gecos)
    name = QString::fromLocal8Bit(pw->pw_gecos).section(',', 0, 0).trimmed();

  // Like git, expand & to the capitalized login name.
  if (name.contains('&') && !login.isEmpty()) {
    QString capitalized = login;
    capitalized[0] = capitalized[0].toUpper();
    name.replace('&', capitalized);
  }

  return !name.isEmpty() ? name : login;
}

} // namespace platform
