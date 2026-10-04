#ifndef QUAKE_CLIENT_THINK_HPP
#define QUAKE_CLIENT_THINK_HPP

namespace quake
{
  /// The two functions the game code has for a player in every frame.
  enum class ClientThink
  {
    /// `PlayerPreThink`, before the player is moved.
    Before,
    /// `PlayerPostThink`, after.
    After,
  };
} // quake

#endif //QUAKE_CLIENT_THINK_HPP
