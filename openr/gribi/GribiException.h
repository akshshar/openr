#pragma once

#include <stdexcept>

#include <folly/Format.h>

namespace openr {

class GribiException : public std::runtime_error {
 public:
  explicit GribiException(const std::string& exception)
      : std::runtime_error(
            folly::sformat("Gribi exception: {} ", exception)) {}
};
} // namespace openr
