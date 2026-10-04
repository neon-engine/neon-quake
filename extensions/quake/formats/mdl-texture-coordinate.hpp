#ifndef QUAKE_MDL_TEXTURE_COORDINATE_HPP
#define QUAKE_MDL_TEXTURE_COORDINATE_HPP

#include <cstdint>

namespace quake
{
  /// Where on the skin a vertex of a model is, in pixels from the left and
  /// from the top.
  ///
  /// A skin holds the front of the model in its left half and the back in
  /// its right half. A vertex on the seam between the two is kept once, with
  /// its place in the left half: a triangle of the back that uses it means
  /// the same place half the width of the skin further right.
  struct MdlTextureCoordinate
  {
    bool on_seam = false;
    std::int32_t s = 0;
    std::int32_t t = 0;
  };
} // quake

#endif //QUAKE_MDL_TEXTURE_COORDINATE_HPP
