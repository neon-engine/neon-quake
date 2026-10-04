#ifndef QUAKE_BSP_CONTENTS_HPP
#define QUAKE_BSP_CONTENTS_HPP

#include <cstdint>

namespace quake
{
  /// What fills a place of a level, with the numbers the file has for it.
  ///
  /// A level also knows water that flows to one of six sides, -9 to -14. The
  /// game treats it as water wherever it asks what fills a place, and so it
  /// is water here.
  enum class BspContents : std::int32_t
  {
    Empty = -1,
    Solid = -2,
    Water = -3,
    Slime = -4,
    Lava = -5,
    Sky = -6,
  };
} // quake

#endif //QUAKE_BSP_CONTENTS_HPP
