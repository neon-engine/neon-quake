#ifndef QUAKE_LEVEL_CALLER_HPP
#define QUAKE_LEVEL_CALLER_HPP

#include <cstdint>

namespace quake
{
  /// Who calls a function of the game code for an entity while a level
  /// runs: with the time of the level, and with the failure of a run that
  /// was stopped kept for the host. `LevelRunning` is one.
  ///
  /// What moves the entities asks it to, when one touches another or is in
  /// the way of a door, without knowing who keeps the time.
  class LevelCaller
  {
  public:
    virtual ~LevelCaller() = default;

    /// Calls a function with the globals `self` and `other` set to two
    /// entities and `time` to the time of the level. False when the run
    /// was stopped. Nothing is run for function 0, which is no function.
    virtual bool RunFunction(std::int32_t function, std::int32_t self, std::int32_t other) = 0;
  };
} // quake

#endif //QUAKE_LEVEL_CALLER_HPP
