#ifndef QUAKE_MDL_MESH_HPP
#define QUAKE_MDL_MESH_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "mdl-file.hpp"
#include "mdl-mesh-vertex.hpp"
#include "mdl-pose.hpp"
#include "mdl-vector.hpp"

namespace quake
{
  /// What a renderer draws of a model: a list of vertices for each pose and
  /// one list of indices for all of them.
  ///
  /// The vertices are not those of the file. A vertex on the seam of the
  /// skin is used by triangles of the front and of the back, which paint it
  /// from two places of the skin, half its width apart. A renderer wants one
  /// place on the skin for a vertex, so such a vertex is here twice. The
  /// first vertices are those of the file, in their order; the second ones
  /// of the seam follow.
  ///
  /// The list of vertices of every pose has the same length and the same
  /// order, and the indices hold for all of them, so a renderer can swap one
  /// pose for another, or blend two, vertex by vertex.
  ///
  /// Everything is in the units and axes of the game: x forward, y left, z
  /// up. Making the axes of an engine of them is for whoever draws it.
  ///
  /// The triangles are in the order of the file, which goes around clockwise
  /// when a triangle is seen from outside the model. A renderer that takes
  /// counter-clockwise as the front either says so the other way around or
  /// swaps two indices of every triangle. The normals point outside.
  class MdlMesh
  {
    MdlVector _scale;
    MdlVector _translate;
    std::size_t _file_vertex_count = 0;

    /// Three for each triangle, into the vertices handed out.
    std::vector<std::uint32_t> _indices;

    /// For each vertex handed out, the vertex of the file it is.
    std::vector<std::uint32_t> _file_vertices;

    /// For each vertex handed out, where on the skin it is, u and v.
    std::vector<std::array<float, 2>> _texture_coordinates;

  public:
    /// An empty mesh, of no vertices and no triangles.
    MdlMesh() = default;

    /// Makes of a model that was read what does not change from pose to
    /// pose. The model is not kept.
    explicit MdlMesh(const MdlFile &file);

    /// Three for each triangle, the same for every pose.
    [[nodiscard]] const std::vector<std::uint32_t> &GetIndices() const;

    /// How many vertices every pose has here, which is those of the file
    /// and the second ones of the seam.
    [[nodiscard]] std::size_t GetVertexCount() const;

    /// For each vertex handed out, the number of the vertex of the file it
    /// was made of.
    [[nodiscard]] const std::vector<std::uint32_t> &GetFileVertices() const;

    /// The vertices of one pose of the model the mesh was made of, see
    /// `MdlFile::FindPose`. A normal is the average of the normals of the
    /// triangles around the vertex, each counting by its area; the two of a
    /// vertex on the seam are the same. A vertex no triangle with an area
    /// uses has the normal straight up. Empty when the pose does not have
    /// as many vertices as the model.
    [[nodiscard]] std::vector<MdlMeshVertex> MakeVertices(const MdlPose &pose) const;
  };
} // quake

#endif //QUAKE_MDL_MESH_HPP
