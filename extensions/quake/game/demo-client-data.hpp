#ifndef QUAKE_DEMO_CLIENT_DATA_HPP
#define QUAKE_DEMO_CLIENT_DATA_HPP

#include <cstdint>

#include "level-vector.hpp"

namespace quake
{
  /// What a server says of the player itself in every packet: where the
  /// eyes are, how the view is kicked, and what the status bar shows.
  /// A value that was not sent is its default here.
  struct DemoClientData
  {
    /// How far the eyes are above the origin of the player.
    static constexpr float default_view_height = 22.0f;

    float view_height = default_view_height;

    /// The pitch the view drifts to on a slope, in degrees.
    float ideal_pitch = 0.0f;

    /// The kick of the view, as of a shot or a hit: pitch, yaw, and roll
    /// in degrees, on top of where the player looks.
    LevelVector punch_angles{};

    /// How fast the player moves, in units a second, in steps of 16.
    LevelVector velocity{};

    /// Everything carried, bits of `QcItem`, the runes among them.
    std::uint32_t items = 0;

    bool is_on_ground = false;
    bool is_in_water = false;

    /// The frame of the model of the weapon in the hands.
    std::int32_t weapon_frame = 0;

    std::int32_t armor = 0;

    /// The model of the weapon in the hands, as an index into the names
    /// of DemoServerInfo. Zero is none.
    std::int32_t weapon_model = 0;

    std::int32_t health = 0;

    /// How much there is of what the weapon in the hands uses.
    std::int32_t ammo = 0;

    std::int32_t shells = 0;
    std::int32_t nails = 0;
    std::int32_t rockets = 0;
    std::int32_t cells = 0;

    /// The weapon in the hands: the low byte of its bit of `QcItem`. The
    /// axe, whose bit is higher, is 0.
    std::uint32_t active_weapon = 0;

    /// How much the weapon in the hands is seen through, as
    /// DemoEntityState::alpha.
    std::int32_t weapon_alpha = 0;
  };
} // quake

#endif //QUAKE_DEMO_CLIENT_DATA_HPP
