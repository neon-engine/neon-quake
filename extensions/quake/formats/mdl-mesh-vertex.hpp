#ifndef QUAKE_MDL_MESH_VERTEX_HPP
#define QUAKE_MDL_MESH_VERTEX_HPP

#include <cstdint>

#include "mdl-vector.hpp"

namespace quake
{
  /// A vertex of a model as a renderer draws it, in one pose.
  struct MdlMeshVertex
  {
    /// The place, in the units and axes of the game: z is up.
    MdlVector position;

    /// The direction away from the surface, of length one, computed from
    /// the triangles around the vertex in this pose.
    MdlVector normal;

    /// Where on the skin the vertex is, from 0 at the left and at the top
    /// to 1 at the right and at the bottom. The same in every pose.
    float u = 0.0f;
    float v = 0.0f;

    /// The normal the file has for the vertex in this pose: the number of a
    /// direction in the table of 162 of the original game, for whoever has
    /// that table and wants its shading.
    std::uint8_t normal_index = 0;
  };
} // quake

#endif //QUAKE_MDL_MESH_VERTEX_HPP
