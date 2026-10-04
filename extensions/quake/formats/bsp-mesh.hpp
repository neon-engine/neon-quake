#ifndef QUAKE_BSP_MESH_HPP
#define QUAKE_BSP_MESH_HPP

#include <cstddef>
#include <string>
#include <vector>

#include "bsp-file.hpp"
#include "bsp-mesh-face.hpp"
#include "bsp-mesh-group.hpp"
#include "bsp-mesh-vertex.hpp"

namespace quake
{
  /// What a renderer draws of a model of a level: the polygons of its faces
  /// as triangles, grouped by texture, and the lightmap of every face.
  ///
  /// It is made from a `BspFile` and keeps nothing of it. Everything stays
  /// in the units and axes of the game, with Z pointing up: turning it into
  /// the axes of the engine is for the code that talks to the engine. The
  /// lightmaps stay one for each face too: packing them into one picture is
  /// for that code as well, which moves the lightmap coordinates of the
  /// vertices of a face to where it put the lightmap of that face.
  ///
  /// The triangles of a face are a fan around its first corner, and keep
  /// the order of the corners of the file: clockwise, seen from the side
  /// the face looks to.
  struct BspMesh
  {
    /// The size of the texture the game draws in place of one a level names
    /// and does not carry, which texture coordinates are counted in then.
    static constexpr float stand_in_texture_size = 16.0f;

    /// How many units of the level lie between two samples of a lightmap.
    static constexpr int units_for_each_sample = 16;

    /// The corners of all faces, face after face.
    std::vector<BspMeshVertex> vertices;

    /// The triangles, one group for each texture that is used, in the order
    /// of the textures of the level.
    std::vector<BspMeshGroup> groups;

    /// The faces of the model, in the order of the level, also those of the
    /// sky and of liquids.
    std::vector<BspMeshFace> faces;

    /// Makes the mesh of a model of a level. Model 0 is the world. Returns
    /// false, says why in `error`, and stays as it was when there is no such
    /// model, or when a face names what is not there, has fewer than three
    /// corners, or has a lightmap that does not lie inside the lighting.
    bool Build(const BspFile &file, std::size_t model, std::string &error);
  };
} // quake

#endif //QUAKE_BSP_MESH_HPP
