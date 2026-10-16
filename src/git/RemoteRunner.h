//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#ifndef REMOTERUNNER_H
#define REMOTERUNNER_H

#include <functional>

namespace git {

class RemoteRunner {

public:
  bool isBusy() const;
  void queue(std::function<void()> fn);
};

} // namespace git

#endif