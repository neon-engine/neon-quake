#ifndef QUAKE_QC_ITEM_HPP
#define QUAKE_QC_ITEM_HPP

#include <cstdint>

namespace quake
{
  /// What a player carries, the bits the game code keeps in the field
  /// `items`, with the four runes above them.
  ///
  /// The game code knows the bits up to `Quad`. The runes are not in the
  /// field: the game code keeps them in the global `serverflags`, as its
  /// lowest four bits, and the engine of the original puts those four on
  /// top of the items, at bit 28, before it tells the status bar. So the
  /// number here has 32 bits and no sign, which a float of the game code
  /// can not hold: see PlayerStats::Read().
  enum class QcItem : std::uint32_t
  {
    // The weapons, in the order the status bar shows them.
    Shotgun = 1u << 0,
    SuperShotgun = 1u << 1,
    Nailgun = 1u << 2,
    SuperNailgun = 1u << 3,
    GrenadeLauncher = 1u << 4,
    RocketLauncher = 1u << 5,
    Lightning = 1u << 6,
    /// A weapon the game was to have and has not.
    SuperLightning = 1u << 7,

    // Which ammunition the weapon in the hands uses.
    Shells = 1u << 8,
    Nails = 1u << 9,
    Rockets = 1u << 10,
    Cells = 1u << 11,

    /// The axe, which has no picture in the status bar.
    Axe = 1u << 12,

    // The armour worn: green, yellow, red.
    Armor1 = 1u << 13,
    Armor2 = 1u << 14,
    Armor3 = 1u << 15,

    SuperHealth = 1u << 16,

    // The keys: silver, gold.
    Key1 = 1u << 17,
    Key2 = 1u << 18,

    // What lasts for a while.
    Invisibility = 1u << 19,
    Invulnerability = 1u << 20,
    Suit = 1u << 21,
    Quad = 1u << 22,

    // The runes, one for each episode that is done.
    Sigil1 = 1u << 28,
    Sigil2 = 1u << 29,
    Sigil3 = 1u << 30,
    Sigil4 = 1u << 31,
  };

  /// Whether the bits of what a player carries have an item.
  [[nodiscard]] constexpr bool HasItem(const std::uint32_t items, const QcItem item)
  {
    return (items & static_cast<std::uint32_t>(item)) != 0;
  }
} // quake

#endif //QUAKE_QC_ITEM_HPP
