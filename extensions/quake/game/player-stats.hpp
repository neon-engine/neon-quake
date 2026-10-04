#ifndef QUAKE_PLAYER_STATS_HPP
#define QUAKE_PLAYER_STATS_HPP

#include <cstdint>
#include <string>

#include "formats/qc-machine.hpp"
#include "qc-fields.hpp"
#include "qc-globals.hpp"

namespace quake
{
  /// What the status bar and the screens between levels show of a player
  /// and of the level: the numbers the engine of the original sent its
  /// client as `stats`, and the two names of the level.
  ///
  /// They are whole numbers, as the original sent them, though the game
  /// code keeps floats: a health of 0.5 is shown, and counts, as 0.
  ///
  /// ```
  /// const PlayerStats stats = PlayerStats::Read(machine, fields, globals, player);
  /// bar.Watch(stats, stats.time);
  /// const std::vector<HudPicture> pictures = bar.Layout(stats, stats.time, {});
  /// ```
  struct PlayerStats
  {
    std::int32_t health = 0;
    std::int32_t armor = 0;

    /// How much there is of what the weapon in the hands uses.
    std::int32_t ammo = 0;

    std::int32_t shells = 0;
    std::int32_t nails = 0;
    std::int32_t rockets = 0;
    std::int32_t cells = 0;

    /// The weapon in the hands: its one bit of `QcItem`, or 0 for none.
    std::uint32_t weapon = 0;

    /// Everything carried, bits of `QcItem`, the runes among them.
    std::uint32_t items = 0;

    /// The share of the damage the armour takes, from 0 to 1. The status
    /// bar does not read it: which armour it shows is in `items`.
    float armor_type = 0.0f;

    std::int32_t killed_monsters = 0;
    std::int32_t total_monsters = 0;
    std::int32_t found_secrets = 0;
    std::int32_t total_secrets = 0;

    /// The name of the level for a reader, which is the `message` of the
    /// world. Its bytes are letters of the console, see `HudText`.
    std::string level_name;

    /// The name of the file of the level, such as `e1m1`.
    std::string map_name;

    /// For how many seconds the level has run.
    float time = 0.0f;

    /// Reads them for a player from the game code.
    ///
    /// The runes come from the global `serverflags` and go on top of the
    /// bits of the field `items`, as the original packs them. A field or a
    /// global the program lacks, and a player that there is not, read as
    /// zero. A float that is no number, or too large for a whole one, is
    /// read as zero too.
    [[nodiscard]] static PlayerStats Read(
      QcMachine &machine,
      const QcFields &fields,
      const QcGlobals &globals,
      std::int32_t player);
  };
} // quake

#endif //QUAKE_PLAYER_STATS_HPP
