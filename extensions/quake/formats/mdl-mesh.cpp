#include "mdl-mesh.hpp"

#include <cmath>

namespace quake
{
  // Helpers of MdlMesh: the little it needs of the mathematics of vectors.
  namespace
  {
    /// A vertex of the file that has no second one for the back.
    constexpr std::uint32_t none = 0xffffffff;

    MdlVector subtract(const MdlVector &a, const MdlVector &b)
    {
      return {a.x - b.x, a.y - b.y, a.z - b.z};
    }

    MdlVector cross(const MdlVector &a, const MdlVector &b)
    {
      return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
    }
  }

  MdlMesh::MdlMesh(const MdlFile &file)
  {
    const MdlHeader &header = file.GetHeader();
    const auto &coordinates = file.GetTextureCoordinates();

    _scale = header.scale;
    _translate = header.translate;
    _file_vertex_count = coordinates.size();

    const auto width = static_cast<float>(header.skin_width);
    const auto height = static_cast<float>(header.skin_height);

    // The middle of a pixel is half a pixel from its edge, which is what
    // keeps a vertex at the edge of the skin from taking its colour from
    // the other side.
    const auto place_on_skin = [&](const std::int32_t s, const std::int32_t t)
    {
      return std::array<float, 2>{(static_cast<float>(s) + 0.5f) / width, (static_cast<float>(t) + 0.5f) / height};
    };

    _file_vertices.reserve(_file_vertex_count);
    _texture_coordinates.reserve(_file_vertex_count);
    for (std::size_t i = 0; i < _file_vertex_count; i++)
    {
      _file_vertices.push_back(static_cast<std::uint32_t>(i));
      _texture_coordinates.push_back(place_on_skin(coordinates[i].s, coordinates[i].t));
    }

    // the second one of each vertex on the seam, made when a triangle of
    // the back first asks for it
    std::vector<std::uint32_t> back_vertices(_file_vertex_count, none);

    _indices.reserve(file.GetTriangles().size() * 3);
    for (const MdlTriangle &triangle : file.GetTriangles())
    {
      for (const std::int32_t corner : triangle.vertices)
      {
        const auto vertex = static_cast<std::uint32_t>(corner);
        const MdlTextureCoordinate &coordinate = coordinates[vertex];

        if (triangle.faces_front || !coordinate.on_seam)
        {
          _indices.push_back(vertex);
          continue;
        }

        if (back_vertices[vertex] == none)
        {
          back_vertices[vertex] = static_cast<std::uint32_t>(_file_vertices.size());
          _file_vertices.push_back(vertex);
          _texture_coordinates.push_back(place_on_skin(coordinate.s + header.skin_width / 2, coordinate.t));
        }
        _indices.push_back(back_vertices[vertex]);
      }
    }
  }

  const std::vector<std::uint32_t> &MdlMesh::GetIndices() const
  {
    return _indices;
  }

  std::size_t MdlMesh::GetVertexCount() const
  {
    return _file_vertices.size();
  }

  const std::vector<std::uint32_t> &MdlMesh::GetFileVertices() const
  {
    return _file_vertices;
  }

  std::vector<MdlMeshVertex> MdlMesh::MakeVertices(const MdlPose &pose) const
  {
    if (pose.vertices.size() != _file_vertex_count) { return {}; }

    std::vector<MdlVector> positions;
    positions.reserve(_file_vertex_count);
    for (const MdlPackedVertex &packed : pose.vertices)
    {
      positions.push_back({
        static_cast<float>(packed.position[0]) * _scale.x + _translate.x,
        static_cast<float>(packed.position[1]) * _scale.y + _translate.y,
        static_cast<float>(packed.position[2]) * _scale.z + _translate.z,
      });
    }

    // The normals are added up for the vertices of the file, not for those
    // handed out, so that the two of a vertex on the seam get the same and
    // no edge shows there.
    std::vector<MdlVector> normals(_file_vertex_count);
    for (std::size_t i = 0; i + 2 < _indices.size(); i += 3)
    {
      const std::uint32_t a = _file_vertices[_indices[i]];
      const std::uint32_t b = _file_vertices[_indices[i + 1]];
      const std::uint32_t c = _file_vertices[_indices[i + 2]];

      // The corners go around clockwise seen from outside, so it is the
      // third one that comes first here for the normal to point outside.
      // It is as long as twice the area of the triangle, which is what
      // makes a large triangle count for more.
      const MdlVector normal = cross(subtract(positions[c], positions[a]), subtract(positions[b], positions[a]));

      for (const std::uint32_t corner : {a, b, c})
      {
        normals[corner].x += normal.x;
        normals[corner].y += normal.y;
        normals[corner].z += normal.z;
      }
    }

    for (MdlVector &normal : normals)
    {
      const float length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
      if (length > 0.0f && std::isfinite(length))
      {
        normal = {normal.x / length, normal.y / length, normal.z / length};
      } else
      {
        normal = {0.0f, 0.0f, 1.0f};
      }
    }

    std::vector<MdlMeshVertex> vertices;
    vertices.reserve(_file_vertices.size());
    for (std::size_t i = 0; i < _file_vertices.size(); i++)
    {
      const std::uint32_t file_vertex = _file_vertices[i];

      MdlMeshVertex vertex;
      vertex.position = positions[file_vertex];
      vertex.normal = normals[file_vertex];
      vertex.u = _texture_coordinates[i][0];
      vertex.v = _texture_coordinates[i][1];
      vertex.normal_index = pose.vertices[file_vertex].normal_index;
      vertices.push_back(vertex);
    }
    return vertices;
  }
} // quake
