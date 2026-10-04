#ifndef QUAKE_PLAYER_MOVEMENT_SETTINGS_HPP
#define QUAKE_PLAYER_MOVEMENT_SETTINGS_HPP

#include <string_view>

#include "qc-console-variables.hpp"

namespace quake
{
  /// The numbers a player is steered by, each at what the original starts
  /// the console variable of the same meaning with. The feel of the game
  /// is in them, and in the order `PlayerMovement` uses them in.
  ///
  /// How fast things fall is not among them: that is the mover's, see
  /// LevelPhysics::SetGravity().
  struct PlayerMovementSettings
  {
    /// The fastest a player asks to go, `sv_maxspeed`.
    float max_speed = 320.0f;

    /// How fast a player on the ground gets to the speed asked for,
    /// `sv_accelerate`.
    float accelerate = 10.0f;

    /// How fast a player on the ground or in water is slowed down,
    /// `sv_friction`.
    float friction = 4.0f;

    /// Under this speed a player on the ground is slowed down as if as
    /// fast as it, so that the player comes to a stop, `sv_stopspeed`.
    float stop_speed = 100.0f;

    /// By how much more a player is slowed down whose way leads over an
    /// edge, `edgefriction`.
    float edge_friction = 2.0f;

    /// The most a player in the air gains along the direction asked for
    /// in a step, whatever the speed asked for. The original has the
    /// number in its code. It is what lets a player turn in the air, and
    /// get faster by it.
    float air_speed = 30.0f;

    /// How far a player leans to the side when moving sideways, in
    /// degrees, and the speed sideways at which the player leans all of
    /// it: `cl_rollangle` and `cl_rollspeed`.
    float roll_angle = 2.0f;
    float roll_speed = 200.0f;

    /// By how many degrees a second a view that was kicked comes back.
    float punch_recovery = 10.0f;

    /// The settings as console variables have them: `sv_maxspeed`,
    /// `sv_accelerate`, `sv_friction`, `sv_stopspeed`, `edgefriction`,
    /// `cl_rollangle`, and `cl_rollspeed`. One that was never set stays at
    /// what the original starts it with.
    [[nodiscard]] static PlayerMovementSettings From(const QcConsoleVariables &variables)
    {
      PlayerMovementSettings settings;
      const auto read = [&variables](const std::string_view name, float &value)
      {
        if (variables.Has(name)) { value = variables.GetFloat(name); }
      };
      read("sv_maxspeed", settings.max_speed);
      read("sv_accelerate", settings.accelerate);
      read("sv_friction", settings.friction);
      read("sv_stopspeed", settings.stop_speed);
      read("edgefriction", settings.edge_friction);
      read("cl_rollangle", settings.roll_angle);
      read("cl_rollspeed", settings.roll_speed);
      return settings;
    }
  };
} // quake

#endif //QUAKE_PLAYER_MOVEMENT_SETTINGS_HPP
