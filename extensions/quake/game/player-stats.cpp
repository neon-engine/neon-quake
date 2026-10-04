#include "player-stats.hpp"

namespace quake
{
  // What turns a float of the game code into a whole number.
  namespace
  {
    /// The whole number of a float, cut towards zero as the original cuts
    /// it. Zero for what is no number or does not fit: to cast such a float
    /// is not defined.
    std::int32_t to_whole(const float value)
    {
      constexpr float limit = 2147483648.0f;
      if (!(value > -limit && value < limit)) { return 0; }

      return static_cast<std::int32_t>(value);
    }
  }

  PlayerStats PlayerStats::Read(
    QcMachine &machine,
    const QcFields &fields,
    const QcGlobals &globals,
    const std::int32_t player)
  {
    PlayerStats stats;
    stats.health = to_whole(fields.health.Get(machine, player));
    stats.armor = to_whole(fields.armorvalue.Get(machine, player));
    stats.ammo = to_whole(fields.currentammo.Get(machine, player));
    stats.shells = to_whole(fields.ammo_shells.Get(machine, player));
    stats.nails = to_whole(fields.ammo_nails.Get(machine, player));
    stats.rockets = to_whole(fields.ammo_rockets.Get(machine, player));
    stats.cells = to_whole(fields.ammo_cells.Get(machine, player));
    stats.weapon = static_cast<std::uint32_t>(to_whole(fields.weapon.Get(machine, player)));
    stats.armor_type = fields.armortype.Get(machine, player);

    // the runes are the lowest four bits of the flags, and the highest
    // four of the items
    const auto items = static_cast<std::uint32_t>(to_whole(fields.items.Get(machine, player)));
    const auto flags = static_cast<std::uint32_t>(to_whole(globals.serverflags.Get(machine)));
    stats.items = items | (flags << 28);

    stats.killed_monsters = to_whole(globals.killed_monsters.Get(machine));
    stats.total_monsters = to_whole(globals.total_monsters.Get(machine));
    stats.found_secrets = to_whole(globals.found_secrets.Get(machine));
    stats.total_secrets = to_whole(globals.total_secrets.Get(machine));

    // the world is entity 0
    stats.level_name = fields.message.GetText(machine, 0);
    stats.map_name = globals.mapname.GetText(machine);
    stats.time = globals.time.Get(machine);
    return stats;
  }
} // quake
