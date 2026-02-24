#pragma once

#include "common/api.h"
#include "exception.h"

namespace lbug {
namespace common {

class LBUG_API CopyException : public Exception {
public:
    explicit CopyException(const std::string& msg)
        : Exception("Copy exception: " + msg,
              true /* skipBacktrace: often caught and rethrown */){};
};

} // namespace common
} // namespace lbug
