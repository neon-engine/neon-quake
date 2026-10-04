#ifndef QUAKE_LEVEL_FAILURE_HPP
#define QUAKE_LEVEL_FAILURE_HPP

#include <cstdint>
#include <string>

#include "formats/qc-error.hpp"

namespace quake
{
  /// A run of the game code that was stopped while a level was started or
  /// was running: for which entity, in which function, and why.
  ///
  /// The level goes on after one. An error the game code raises itself,
  /// with its builtins `error` and `objerror`, comes here as well when the
  /// host's builtin stops the run for it; the message tells the two apart.
  struct LevelFailure
  {
    /// The entity the function ran for, and its classname then.
    std::int32_t entity = 0;
    std::string classname;

    /// The name of the function that was called.
    std::string function;

    QcError error;
  };
} // quake

#endif //QUAKE_LEVEL_FAILURE_HPP
