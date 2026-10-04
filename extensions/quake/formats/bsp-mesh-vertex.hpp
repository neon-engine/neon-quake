#ifndef QUAKE_BSP_MESH_VERTEX_HPP
#define QUAKE_BSP_MESH_VERTEX_HPP

#include "bsp-vector.hpp"

namespace quake
{
  /// A corner of a face of a level, with what a renderer needs there.
  ///
  /// The place and the normal are in the units and axes of the game, with Z
  /// pointing up.
  struct BspMeshVertex
  {
    BspVector position;

    /// The direction its face looks in.
    BspVector normal;

    /// Where it lies on the texture, in widths and heights of it: 0 is the
    /// left or the top of the picture, 1 the right or the bottom, and what
    /// is beyond repeats it.
    float texture_u = 0.0f;
    float texture_v = 0.0f;

    /// Where it lies on the lightmap of its own face, from 0 at the left or
    /// top border of the lightmap to 1 at the right or bottom border. The
    /// middle of the first sample is at half a sample from the border.
    float lightmap_u = 0.0f;
    float lightmap_v = 0.0f;
  };
} // quake

#endif //QUAKE_BSP_MESH_VERTEX_HPP
