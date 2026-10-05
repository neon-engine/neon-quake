#ifndef QUAKE_STATUS_BAR_HPP
#define QUAKE_STATUS_BAR_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "hud-picture.hpp"
#include "player-stats.hpp"
#include "status-bar-options.hpp"

namespace quake
{
  /// The status bar of the original as a list of pictures to draw: which
  /// pictures of `gfx.wad` go where for what a player has.
  ///
  /// The status bar itself, `sbar`, is the lowest 24 rows: the armour and
  /// its number, the face, the health, and the ammunition of the weapon
  /// in the hands with its number. Above it is the bar of what is carried,
  /// `ibar`: the weapons, the four counts of ammunition, the keys, what
  /// lasts for a while, and the runes. All places are those of the
  /// original, in the screen `HudPicture` describes, with
  /// `HudAnchor::Bottom`.
  ///
  /// Two things the bar shows are not in the stats, since they are about
  /// when something happened: a weapon that was just picked up flashes
  /// for a second, and the face is in pain for a moment after a hit. The
  /// bar remembers both, when it is told: Watch() with the stats of every
  /// frame, and ShowPain() when the player was hurt. A bar that is told
  /// neither shows everything at rest.
  ///
  /// ```
  /// StatusBar bar;
  /// // every frame
  /// const PlayerStats stats = PlayerStats::Read(machine, fields, globals, player);
  /// bar.Watch(stats, stats.time);
  /// if (was_hurt) { bar.ShowPain(stats.time); }
  /// for (const HudPicture &picture : bar.Layout(stats, stats.time, options)) { ... }
  /// ```
  class StatusBar final
  {
    /// How many bits the items have.
    static constexpr std::size_t item_count = 32;

    /// A time before every time, for what never happened.
    static constexpr float never = -1.0e30f;

    /// When each bit of the items was last seen to be new.
    std::array<float, item_count> _got_at;

    /// The items of the stats last watched.
    std::uint32_t _items = 0;

    /// Until when the face is in pain.
    float _pain_until = never;

    /// The bar of what is carried.
    void AddInventory(std::vector<HudPicture> &pictures, const PlayerStats &stats, float time) const;

    /// The status bar itself.
    void AddBar(std::vector<HudPicture> &pictures, const PlayerStats &stats, float time) const;

  public:
    /// The y of the upper edge of the status bar, and of the bar of what
    /// is carried, in the screen of 320 by 200.
    static constexpr std::int32_t top = 176;
    static constexpr std::int32_t inventory_top = 152;

    /// How many rows each of the two bars has.
    static constexpr std::int32_t height = 24;

    /// For how long the face is in pain after a hit.
    static constexpr float pain_seconds = 0.2f;

    /// For how long a weapon flashes after it was picked up. It goes
    /// through five pictures, each a tenth of a second, twice.
    static constexpr float flash_seconds = 1.0f;

    /// At and under these the number of the health, of the armour, and of
    /// the ammunition is red.
    static constexpr std::int32_t low_health = 25;
    static constexpr std::int32_t low_armor = 25;
    static constexpr std::int32_t low_ammo = 10;

    /// How much the backgrounds of the bars, `sbar`, `ibar`, and `scorebar`,
    /// cover the view behind them: see-through, as QuakeSpasm and vkQuake
    /// draw them by default (`scr_sbaralpha`). What is on them, the numbers,
    /// the face, and the icons, covers all.
    static constexpr float background_opacity = 0.75f;

    StatusBar();

    /// Looks at the stats of a frame for items that are new since the
    /// frame before, and remembers the time for each: that is when a
    /// weapon starts to flash. What a player has when a level starts is
    /// new as well, and flashes, as in the original.
    void Watch(const PlayerStats &stats, float time);

    /// Says that the player was hurt at a time: the face is in pain until
    /// `pain_seconds` later. The original did so for every message of
    /// damage, which the game code asks for with the fields `dmg_take`
    /// and `dmg_save` of a player.
    void ShowPain(float time);

    /// Forgets what was watched, for a new game.
    void Forget();

    /// What to draw at a time, in order.
    ///
    /// With the scores asked for, or a player whose health is 0 or less,
    /// the status bar gives its place to `Scoreboard`, whatever the size.
    /// Else it is drawn unless the size is `StatusBarSize::None`. The bar
    /// of what is carried is drawn above either when the size is
    /// `StatusBarSize::Full`.
    ///
    /// `time` is on the clock Watch() and ShowPain() were told, the time
    /// of the level. A time before one of theirs, as after a level began
    /// anew, shows that weapon and the face at rest.
    [[nodiscard]] std::vector<HudPicture> Layout(
      const PlayerStats &stats,
      float time,
      const StatusBarOptions &options) const;
  };
} // quake

#endif //QUAKE_STATUS_BAR_HPP
