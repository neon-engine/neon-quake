#ifndef QUAKE_HUD_PICTURE_KIND_HPP
#define QUAKE_HUD_PICTURE_KIND_HPP

#include <cstdint>

namespace quake
{
  /// What a `HudPicture` asks to be drawn.
  enum class HudPictureKind : std::uint8_t
  {
    /// A whole picture, found by its name.
    Picture,

    /// One letter of the sheet of letters of the console, 8 by 8 pixels.
    Character,
  };
} // quake

#endif //QUAKE_HUD_PICTURE_KIND_HPP
