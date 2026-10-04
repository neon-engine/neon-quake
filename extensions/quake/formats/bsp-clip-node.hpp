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
    /// A level with more than 32767 clip nodes keeps the numbers above that
    /// as negative ones too: read without a sign, a child that is the
    /// number of a clip node that is there is that clip node. `BspHull`
    /// reads them so.
    std::array<std::int16_t, 2> children{};
  };
} // quake

#endif //QUAKE_BSP_CLIP_NODE_HPP
