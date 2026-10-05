#ifndef QUAKE_HUD_PICTURE_HPP
#define QUAKE_HUD_PICTURE_HPP

#include <cstdint>
#include <string_view>

#include "hud-anchor.hpp"
#include "hud-picture-kind.hpp"

namespace quake
{
  /// One thing to draw over the view: a picture, or a letter, at a place.
  /// The layouts of the status bar and of the screens of the game give
  /// lists of them, to be drawn in the order of the list, later ones over
  /// earlier ones.
  ///
  /// A place is counted in the screen of the original, 320 pixels wide
  /// and 200 high, from its upper left corner: x to the right, y down, and
  /// `x`, `y` is where the upper left corner of the picture goes. The
  /// status bar is the lowest 24 rows, so its upper edge is y 176, and the
  /// bar of what is carried is the 24 rows above it, from y 152. This is
  /// what the original's `Sbar_DrawPic` made of its x and y, which it
  /// counted from the upper left corner of the status bar: x stays, and y
  /// has 176 added.
  ///
  /// A real screen is larger. `anchor` says where the 320 by 200 go on it
  /// once they are scaled: a picture at x, y, drawn at a scale s on a
  /// screen W wide and H high, has its corner at
  ///
  /// - `(W - 320 s) / 2 + x s` from the left, for the two that follow,
  /// - `H - (200 - y) s` from the top for `HudAnchor::Bottom`,
  /// - `(H - 200 s) / 2 + y s` from the top for `HudAnchor::Center`,
  ///
  /// and is s times as wide and high as it has pixels. `HudAnchor::TopLeft`
  /// counts from the corner of the real screen, at a scale of its own.
  struct HudPicture
  {
    /// The name of the lump that holds the letters, in `gfx.wad`.
    static constexpr std::string_view characters_name = "conchars";

    /// How many pixels wide and high a letter is.
    static constexpr std::int32_t character_size = 8;

    HudPictureKind kind = HudPictureKind::Picture;

    /// Which picture: the name of a lump of `gfx.wad`, such as `sbar`, or,
    /// when it starts with `gfx/`, the name of a file of the paks, such as
    /// `gfx/inter.lmp`. For a letter it is `characters_name`. The names
    /// are constants of the program, so the view stays good for ever.
    std::string_view name;

    /// For a letter, which of the 256 of the sheet: they are 16 in a row
    /// and 16 rows, so letter n is at column `n % 16` and row `n / 16`,
    /// each 8 pixels. Nothing for a picture.
    std::int32_t character = 0;

    std::int32_t x = 0;
    std::int32_t y = 0;

    HudAnchor anchor = HudAnchor::Bottom;

    /// For a picture, the rows of it that are drawn, when not all of it
    /// is: from row `first_row`, counted from its top, so many `rows`. 0
    /// rows is every row from the first on. The upper left corner of the
    /// strip is what goes at `x`, `y`. A menu leaves an item out of a
    /// picture of several this way.
    std::int32_t first_row = 0;
    std::int32_t rows = 0;

    bool operator==(const HudPicture &) const = default;
  };
} // quake

#endif //QUAKE_HUD_PICTURE_HPP
