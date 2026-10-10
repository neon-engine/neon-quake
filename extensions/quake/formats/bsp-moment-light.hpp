#ifndef QUAKE_BSP_MOMENT_LIGHT_HPP
#define QUAKE_BSP_MOMENT_LIGHT_HPP

#include <cstdint>

#include "bsp-vector.hpp"

namespace quake
{
  /// A light that is there for a moment, as `BspLightVisibility` is asked
  /// about it: an explosion, the flash of a shot, a rocket on its way.
  ///
  /// It is in the units and axes of the game, with Z pointing up.
  struct BspMomentLight
  {
    BspVector place;

    /// How far it reaches. Nothing further away is lit by it, so a room
    /// further away is not looked at.
    float radius = 0.0f;

    /// Which bit of the mask of a face stands for it, from 0 to 31.
    std::uint32_t bit = 0;
  };
} // quake

#endif //QUAKE_BSP_MOMENT_LIGHT_HPP
