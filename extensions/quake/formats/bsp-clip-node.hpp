#ifndef QUAKE_BSP_CLIP_NODE_HPP
#define QUAKE_BSP_CLIP_NODE_HPP

#include <array>
#include <cstdint>

namespace quake
{
  /// A fork of the trees that things collide with: a plane, and what lies in
  /// front of it and behind it.
  struct BspClipNode
  {
    std::int32_t plane = 0;

    /// What is in front and behind. A number that is not negative is another
    /// clip node. A negative one is what fills the space there: -1 nothing,
    /// -2 solid, -3 water, -4 slime, -5 lava, -6 sky.
    ///
    /// They are as wide as the widest form of a level has them. A file of
    /// version 29 keeps them in 16 bits, and one with more than 32767 clip
    /// nodes keeps the numbers above that as negative ones: `BspFile` reads
    /// those back into the numbers of the clip nodes they are, so that a
    /// negative child here is always what fills the space.
    std::array<std::int32_t, 2> children{};
  };
} // quake

#endif //QUAKE_BSP_CLIP_NODE_HPP
