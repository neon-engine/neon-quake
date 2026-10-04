#ifndef QUAKE_QC_FLAG_HPP
#define QUAKE_QC_FLAG_HPP

#include <cstdint>

namespace quake
{
  /// What an entity is and how it stands, the bits the game code and the
  /// engine of the original keep in the field `flags`. The field is a float
  /// that holds a whole number, so the bits are read from it as one.
  enum class QcFlag : std::int32_t
  {
    /// A monster that flies: it moves up and down by itself, and does not
    /// fall.
    Fly = 1,
    /// A monster that swims: it stays in the water.
    Swim = 2,
    /// A player.
    Client = 8,
    InWater = 16,
    /// A monster: a missile hits it in a box that is larger than its own.
    Monster = 32,
    GodMode = 64,
    /// No monster looks for this player.
    NoTarget = 128,
    /// Something to pick up: it is touched from further away.
    Item = 256,
    /// It stands on something, which the field `groundentity` names.
    OnGround = 512,
    /// A monster that has a floor under a part of it only, and is let walk
    /// off it.
    PartialGround = 1024,
    WaterJump = 2048,
    JumpReleased = 4096,
  };

  /// Whether the number of a field `flags` has a flag.
  [[nodiscard]] constexpr bool HasFlag(const float flags, const QcFlag flag)
  {
    return (static_cast<std::int32_t>(flags) & static_cast<std::int32_t>(flag)) != 0;
  }

  /// The number of a field `flags` with a flag set.
  [[nodiscard]] constexpr float WithFlag(const float flags, const QcFlag flag)
  {
    return static_cast<float>(static_cast<std::int32_t>(flags) | static_cast<std::int32_t>(flag));
  }

  /// The number of a field `flags` with a flag taken away.
  [[nodiscard]] constexpr float WithoutFlag(const float flags, const QcFlag flag)
  {
    return static_cast<float>(static_cast<std::int32_t>(flags) & ~static_cast<std::int32_t>(flag));
  }
} // quake

#endif //QUAKE_QC_FLAG_HPP
