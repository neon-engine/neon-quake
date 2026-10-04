#ifndef QUAKE_BSP_LIGHT_SAMPLE_HPP
#define QUAKE_BSP_LIGHT_SAMPLE_HPP

#include <cstdint>

#include "bsp-vector.hpp"

namespace quake
{
  /// The light of a level at a place, as `BspLightPoint` finds it under the
  /// place.
  ///
  /// The colours are counted as the samples of a lightmap are: 0 is dark,
  /// 255 is the most one light gives, and a face lit by several lights at
  /// once, or by one that is brighter than usual just now, goes above that.
  /// Nothing is cut off here: whoever draws decides how bright that is.
  struct BspLightSample
  {
    /// Whether a face was found under the place. When none was, the colours
    /// are dark in a level that has light and 255 in one that has none at
    /// all, as in the original game. A face that was found may be dark too:
    /// one that no light of the level reaches.
    bool is_found = false;

    float red = 0.0f;
    float green = 0.0f;
    float blue = 0.0f;

    /// Where the face was met, which is where a shadow of the thing would
    /// lie. It means nothing when no face was found.
    BspVector place;

    /// Which face of the level it was. It means nothing when none was found.
    std::uint32_t face = 0;
  };
} // quake

#endif //QUAKE_BSP_LIGHT_SAMPLE_HPP
