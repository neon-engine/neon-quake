#include "status-bar.hpp"

#include <algorithm>
#include <string_view>

#include "hud-text.hpp"
#include "qc-item.hpp"
#include "scoreboard.hpp"

namespace quake
{
  // The names of the pictures of the bars, in the order of their bits.
  namespace
  {
    constexpr std::size_t weapon_count = 7;

    /// The pictures of each weapon: at rest, in the hands, and the five of
    /// the flash.
    constexpr std::array<std::array<std::string_view, weapon_count>, 7> weapon_names = {{
      {
        "inv_shotgun", "inv_sshotgun", "inv_nailgun", "inv_snailgun", "inv_rlaunch", "inv_srlaunch",
        "inv_lightng",
      },
      {
        "inv2_shotgun", "inv2_sshotgun", "inv2_nailgun", "inv2_snailgun", "inv2_rlaunch", "inv2_srlaunch",
        "inv2_lightng",
      },
      {
        "inva1_shotgun", "inva1_sshotgun", "inva1_nailgun", "inva1_snailgun", "inva1_rlaunch",
        "inva1_srlaunch", "inva1_lightng",
      },
      {
        "inva2_shotgun", "inva2_sshotgun", "inva2_nailgun", "inva2_snailgun", "inva2_rlaunch",
        "inva2_srlaunch", "inva2_lightng",
      },
      {
        "inva3_shotgun", "inva3_sshotgun", "inva3_nailgun", "inva3_snailgun", "inva3_rlaunch",
        "inva3_srlaunch", "inva3_lightng",
      },
      {
        "inva4_shotgun", "inva4_sshotgun", "inva4_nailgun", "inva4_snailgun", "inva4_rlaunch",
        "inva4_srlaunch", "inva4_lightng",
      },
      {
        "inva5_shotgun", "inva5_sshotgun", "inva5_nailgun", "inva5_snailgun", "inva5_rlaunch",
        "inva5_srlaunch", "inva5_lightng",
      },
    }};

    constexpr std::size_t at_rest = 0;
    constexpr std::size_t in_hands = 1;
    constexpr std::size_t first_flash = 2;
    constexpr std::int32_t flash_pictures = 5;

    /// The keys and what lasts for a while, from the bit of the first key.
    constexpr std::array<std::string_view, 6> item_names = {
      "sb_key1", "sb_key2", "sb_invis", "sb_invuln", "sb_suit", "sb_quad",
    };
    constexpr std::size_t first_item_bit = 17;

    constexpr std::array<std::string_view, 4> sigil_names = {"sb_sigil1", "sb_sigil2", "sb_sigil3", "sb_sigil4"};
    constexpr std::size_t first_sigil_bit = 28;

    /// The faces from the worst health to the best, and the same in pain.
    constexpr std::array<std::string_view, 5> face_names = {"face5", "face4", "face3", "face2", "face1"};
    constexpr std::array<std::string_view, 5> pain_names = {"face_p5", "face_p4", "face_p3", "face_p2", "face_p1"};

    /// The picture of the face of a player: what lasts for a while comes
    /// before the health.
    std::string_view find_face(const PlayerStats &stats, const bool in_pain)
    {
      const bool invisible = HasItem(stats.items, QcItem::Invisibility);
      const bool invulnerable = HasItem(stats.items, QcItem::Invulnerability);
      if (invisible && invulnerable) { return "face_inv2"; }
      if (HasItem(stats.items, QcItem::Quad)) { return "face_quad"; }
      if (invisible) { return "face_invis"; }
      if (invulnerable) { return "face_invul2"; }

      // a band for every 20 of health, and the best from 80 on
      const auto band = static_cast<std::size_t>(std::clamp(stats.health / 20, 0, 4));
      return (in_pain ? pain_names : face_names)[band];
    }
  }

  StatusBar::StatusBar()
  {
    _got_at.fill(never);
  }

  void StatusBar::Watch(const PlayerStats &stats, const float time)
  {
    const std::uint32_t added = stats.items & ~_items;
    for (std::size_t bit = 0; bit < item_count; bit++)
    {
      if ((added & (1u << bit)) != 0) { _got_at[bit] = time; }
    }
    _items = stats.items;
  }

  void StatusBar::ShowPain(const float time)
  {
    _pain_until = time + pain_seconds;
  }

  void StatusBar::Forget()
  {
    *this = StatusBar();
  }

  void StatusBar::AddInventory(std::vector<HudPicture> &pictures, const PlayerStats &stats, const float time) const
  {
    constexpr std::int32_t icons_top = inventory_top + 8;
    pictures.push_back({.name = "ibar", .x = 0, .y = inventory_top, .opacity = background_opacity});

    for (std::size_t weapon = 0; weapon < weapon_count; weapon++)
    {
      const std::uint32_t bit = 1u << weapon;
      if ((stats.items & bit) == 0) { continue; }

      // a tenth of a second for each picture of the flash, ten of them
      const float since = time - _got_at[weapon];
      std::size_t state = stats.weapon == bit ? in_hands : at_rest;
      if (since >= 0.0f && since < flash_seconds)
      {
        const auto tenth = static_cast<std::int32_t>(since * 10.0f);
        if (tenth < 10) { state = first_flash + static_cast<std::size_t>(tenth % flash_pictures); }
      }
      pictures.push_back({
        .name = weapon_names[state][weapon],
        .x = static_cast<std::int32_t>(weapon) * 24,
        .y = icons_top,
      });
    }

    // the four counts, each three small digits moved to the right
    const std::array<std::int32_t, 4> counts = {stats.shells, stats.nails, stats.rockets, stats.cells};
    for (std::size_t kind = 0; kind < counts.size(); kind++)
    {
      const std::int32_t count = std::clamp(counts[kind], 0, HudText::largest_number);
      const std::array<std::int32_t, 3> digits = {count / 100, count / 10 % 10, count % 10};
      bool started = false;
      for (std::size_t place = 0; place < digits.size(); place++)
      {
        started = started || digits[place] != 0 || place == digits.size() - 1;
        if (!started) { continue; }

        pictures.push_back({
          .kind = HudPictureKind::Character,
          .name = HudPicture::characters_name,
          .character = HudText::small_digits + digits[place],
          .x = static_cast<std::int32_t>(kind) * 48 + 10 + static_cast<std::int32_t>(place) * 8,
          .y = inventory_top,
        });
      }
    }

    for (std::size_t item = 0; item < item_names.size(); item++)
    {
      if ((stats.items & (1u << (first_item_bit + item))) == 0) { continue; }

      pictures.push_back({.name = item_names[item], .x = 192 + static_cast<std::int32_t>(item) * 16, .y = icons_top});
    }

    for (std::size_t sigil = 0; sigil < sigil_names.size(); sigil++)
    {
      if ((stats.items & (1u << (first_sigil_bit + sigil))) == 0) { continue; }

      pictures.push_back({.name = sigil_names[sigil], .x = 288 + static_cast<std::int32_t>(sigil) * 8, .y = icons_top});
    }
  }

  void StatusBar::AddBar(std::vector<HudPicture> &pictures, const PlayerStats &stats, const float time) const
  {
    pictures.push_back({.name = "sbar", .x = 0, .y = top, .opacity = background_opacity});

    // the armour: who can not be hurt has 666 of it, in red
    if (HasItem(stats.items, QcItem::Invulnerability))
    {
      HudText::AddNumber(pictures, 666, 24, top, HudAnchor::Bottom, true);
      pictures.push_back({.name = "disc", .x = 0, .y = top});
    }
    else
    {
      HudText::AddNumber(pictures, stats.armor, 24, top, HudAnchor::Bottom, stats.armor <= low_armor);
      if (HasItem(stats.items, QcItem::Armor3)) { pictures.push_back({.name = "sb_armor3", .x = 0, .y = top}); }
      else if (HasItem(stats.items, QcItem::Armor2)) { pictures.push_back({.name = "sb_armor2", .x = 0, .y = top}); }
      else if (HasItem(stats.items, QcItem::Armor1)) { pictures.push_back({.name = "sb_armor1", .x = 0, .y = top}); }
    }

    const bool in_pain = time >= _pain_until - pain_seconds && time <= _pain_until;
    pictures.push_back({.name = find_face(stats, in_pain), .x = 112, .y = top});

    HudText::AddNumber(pictures, stats.health, 136, top, HudAnchor::Bottom, stats.health <= low_health);

    // the ammunition of the weapon in the hands; the axe uses none, and
    // has no picture, but its count of nothing is drawn
    if (HasItem(stats.items, QcItem::Shells)) { pictures.push_back({.name = "sb_shells", .x = 224, .y = top}); }
    else if (HasItem(stats.items, QcItem::Nails)) { pictures.push_back({.name = "sb_nails", .x = 224, .y = top}); }
    else if (HasItem(stats.items, QcItem::Rockets)) { pictures.push_back({.name = "sb_rocket", .x = 224, .y = top}); }
    else if (HasItem(stats.items, QcItem::Cells)) { pictures.push_back({.name = "sb_cells", .x = 224, .y = top}); }

    HudText::AddNumber(pictures, stats.ammo, 248, top, HudAnchor::Bottom, stats.ammo <= low_ammo);
  }

  std::vector<HudPicture> StatusBar::Layout(
    const PlayerStats &stats,
    const float time,
    const StatusBarOptions &options) const
  {
    std::vector<HudPicture> pictures;
    if (options.size == StatusBarSize::Full) { AddInventory(pictures, stats, time); }

    // A player who is dead sees the counts of the level in place of the
    // status bar, as in the original.
    if (stats.health <= 0)
    {
      const std::vector<HudPicture> scores = Scoreboard::Layout(stats);
      pictures.insert(pictures.end(), scores.begin(), scores.end());
      return pictures;
    }

    if (options.size != StatusBarSize::None) { AddBar(pictures, stats, time); }

    // One who asks for them while playing sees them in the middle of the
    // screen, and keeps the status bar. The original put them over it.
    if (options.shows_scores)
    {
      const std::vector<HudPicture> scores = Scoreboard::Layout(stats, HudAnchor::Center, Scoreboard::middle_top);
      pictures.insert(pictures.end(), scores.begin(), scores.end());
    }

    return pictures;
  }
} // quake
