#ifndef QUAKE_PLAYER_COMMAND_HPP
#define QUAKE_PLAYER_COMMAND_HPP

#include "level-vector.hpp"

namespace quake
{
  /// What a player asks for in one step: how fast to go along each of the
  /// three axes of where the player looks, and where the player looks. It
  /// is what a client of the original sends its server in every frame.
  ///
  /// The moves are speeds in units a second. A key that is held asks for
  /// the whole of it, 200 forward and 350 sideways in the original, twice
  /// that forward when running; a stick asks for a part. They are wishes:
  /// `PlayerMovement` holds them to the fastest a player goes.
  ///
  /// The buttons and the impulse are not here. A host writes them into the
  /// fields `button0`, `button2`, and `impulse` itself, for the game code
  /// to read.
  struct PlayerCommand
  {
    /// Forward when more than zero, back when less.
    float forward_move = 0.0f;

    /// To the right when more than zero, to the left when less.
    float side_move = 0.0f;

    /// Up when more than zero, down when less. Only asked in water, and
    /// by what flies through walls.
    float up_move = 0.0f;

    /// Where the player looks: pitch, yaw, and roll in degrees, as the
    /// field `v_angle` has them. A pitch above zero looks down.
    LevelVector view_angles{};
  };
} // quake

#endif //QUAKE_PLAYER_COMMAND_HPP
