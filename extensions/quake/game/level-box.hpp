#ifndef QUAKE_LEVEL_BOX_HPP
#define QUAKE_LEVEL_BOX_HPP

#include <cstddef>

#include "level-vector.hpp"

namespace quake
{
  /// A box in the level with sides along the axes, by its lowest and its
  /// highest corner.
  struct LevelBox
  {
    LevelVector mins{};
    LevelVector maxs{};

    /// Whether two boxes share a place. Boxes that only meet at a side do.
    [[nodiscard]] constexpr bool Overlaps(const LevelBox &other) const
    {
      for (std::size_t i = 0; i < mins.size(); i++)
      {
        if (mins[i] > other.maxs[i] || maxs[i] < other.mins[i]) { return false; }
      }
      return true;
    }
  };
} // quake

#endif //QUAKE_LEVEL_BOX_HPP
