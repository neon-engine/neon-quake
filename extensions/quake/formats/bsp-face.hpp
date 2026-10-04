#ifndef QUAKE_BSP_FACE_HPP
#define QUAKE_BSP_FACE_HPP

#include <array>
#include <cstdint>

namespace quake
{
  /// A flat polygon of the level, the thing that is drawn.
  struct BspFace
  {
    /// The style that says a face has no further lightmap.
    static constexpr std::uint8_t no_style = 255;

    std::uint16_t plane = 0;

    /// Not zero when the face looks the other way than its plane does.
    std::int16_t side = 0;

    /// Its corners, as entries of the list of edges of faces, in order
    /// around the polygon.
    std::int32_t first_edge = 0;
    std::uint16_t edge_count = 0;

    std::uint16_t texture_info = 0;

    /// What lights each of its lightmaps, up to four, one after the other in
    /// the lighting: 0 is light that stays as it is, 1 to 11 flicker and
    /// pulse, 32 and up are switched by the game. `no_style` ends the list.
    std::array<std::uint8_t, 4> styles{};

    /// Where its lightmaps start in the lighting, or -1 when it has none.
    std::int32_t light_offset = -1;
  };
} // quake

#endif //QUAKE_BSP_FACE_HPP
