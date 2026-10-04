#include "bsp-mesh.hpp"

#include <cmath>
#include <cstdint>
#include <map>
#include <utility>

namespace quake
{
  // Helpers of BspMesh: where a place of the level lies along a direction of
  // a texture, and the sentence that refuses a face.
  namespace
  {
    /// The largest number a corner may have on its texture. It is far more
    /// than any level has, and keeps the sizes of lightmaps inside what a
    /// whole number holds.
    constexpr double largest_extent = 1.0e9;

    /// `place . axis + offset`, counted with more digits than the file has,
    /// as the tools that made the lightmaps count it, so that a corner that
    /// lies on a border of sixteen units is not put a sample too far.
    double along(const BspVector &place, const BspVector &axis, const float offset)
    {
      return static_cast<double>(place.x) * static_cast<double>(axis.x) +
        static_cast<double>(place.y) * static_cast<double>(axis.y) +
        static_cast<double>(place.z) * static_cast<double>(axis.z) +
        static_cast<double>(offset);
    }

    bool refuse(std::string &error, const std::size_t face, const std::string &reason)
    {
      error = "face " + std::to_string(face) + " " + reason;
      return false;
    }
  }

  bool BspMesh::Build(const BspFile &file, const std::size_t model, std::string &error)
  {
    if (model >= file.models.size())
    {
      error = "model " + std::to_string(model) + " was asked for, and there are " +
        std::to_string(file.models.size());
      return false;
    }

    const BspModel &source = file.models[model];
    if (source.first_face < 0 || source.face_count < 0 ||
      static_cast<std::size_t>(source.first_face) > file.faces.size() ||
      static_cast<std::size_t>(source.face_count) > file.faces.size() - static_cast<std::size_t>(source.first_face))
    {
      error = "model " + std::to_string(model) + " names " + std::to_string(source.face_count) + " faces from " +
        std::to_string(source.first_face) + ", and there are " + std::to_string(file.faces.size());
      return false;
    }

    BspMesh mesh;
    // ordered by texture, which is the order the groups are handed out in
    std::map<std::int32_t, BspMeshGroup> groups;

    const auto first_face = static_cast<std::size_t>(source.first_face);
    const std::size_t end_face = first_face + static_cast<std::size_t>(source.face_count);
    for (std::size_t face_index = first_face; face_index < end_face; face_index++)
    {
      const BspFace &face = file.faces[face_index];

      // a level that was read has all of this, one put together by hand may not
      if (face.plane >= file.planes.size()) { return refuse(error, face_index, "names a plane that is not there"); }
      if (face.texture_info >= file.texture_infos.size())
      {
        return refuse(error, face_index, "names a texture info that is not there");
      }
      if (face.edge_count < 3)
      {
        return refuse(error, face_index, "has " + std::to_string(face.edge_count) + " corners, fewer than three");
      }
      if (face.first_edge < 0 ||
        static_cast<std::size_t>(face.first_edge) > file.face_edges.size() ||
        face.edge_count > file.face_edges.size() - static_cast<std::size_t>(face.first_edge))
      {
        return refuse(error, face_index, "names edges of faces that are not there");
      }

      const BspTextureInfo &info = file.texture_infos[face.texture_info];
      if (info.texture < 0 || static_cast<std::size_t>(info.texture) >= file.textures.size())
      {
        return refuse(error, face_index, "names a texture that is not there");
      }
      const std::optional<MipTexture> &texture = file.textures[static_cast<std::size_t>(info.texture)];

      BspMeshFace made;
      made.face = static_cast<std::uint32_t>(face_index);
      made.texture = info.texture;
      made.first_vertex = static_cast<std::uint32_t>(mesh.vertices.size());
      made.vertex_count = face.edge_count;
      made.is_sky = texture.has_value() && texture->IsSky();
      made.is_liquid = texture.has_value() && texture->IsLiquid();
      made.light_styles = face.styles;
      made.light_offset = face.light_offset;

      // the plane looks one way, and a face on its back looks the other
      BspVector normal = file.planes[face.plane].normal;
      if (face.side != 0) { normal = {-normal.x, -normal.y, -normal.z}; }

      const double texture_width = texture.has_value() ? texture->width : stand_in_texture_size;
      const double texture_height = texture.has_value() ? texture->height : stand_in_texture_size;

      // where each corner lies on the texture, in its pixels, and how far
      // the face reaches there
      std::vector<std::pair<double, double>> on_texture;
      on_texture.reserve(face.edge_count);
      double least_s = 0.0;
      double least_t = 0.0;
      double most_s = 0.0;
      double most_t = 0.0;

      for (std::size_t corner = 0; corner < face.edge_count; corner++)
      {
        // an edge of a face is an edge walked forwards, from its first
        // vertex, or, when negative, backwards, from its second
        const std::int64_t face_edge = file.face_edges[static_cast<std::size_t>(face.first_edge) + corner];
        const std::int64_t edge = face_edge < 0 ? -face_edge : face_edge;
        if (static_cast<std::uint64_t>(edge) >= file.edges.size())
        {
          return refuse(error, face_index, "names an edge that is not there");
        }
        const std::uint16_t vertex = file.edges[static_cast<std::size_t>(edge)].vertices[face_edge < 0 ? 1 : 0];
        if (vertex >= file.vertices.size()) { return refuse(error, face_index, "names a vertex that is not there"); }

        const BspVector &position = file.vertices[vertex];
        const double s = along(position, info.s_axis, info.s_offset);
        const double t = along(position, info.t_axis, info.t_offset);
        if (!(std::abs(s) <= largest_extent) || !(std::abs(t) <= largest_extent))
        {
          return refuse(error, face_index, "has a corner that lies nowhere on its texture");
        }

        if (corner == 0 || s < least_s) { least_s = s; }
        if (corner == 0 || s > most_s) { most_s = s; }
        if (corner == 0 || t < least_t) { least_t = t; }
        if (corner == 0 || t > most_t) { most_t = t; }
        on_texture.emplace_back(s, t);

        BspMeshVertex made_vertex;
        made_vertex.position = position;
        made_vertex.normal = normal;
        made_vertex.texture_u = static_cast<float>(s / texture_width);
        made_vertex.texture_v = static_cast<float>(t / texture_height);
        mesh.vertices.push_back(made_vertex);
      }

      // A face that is not special has a lightmap with a sample every
      // sixteen units of its texture, on the lines of sixteen: from the last
      // line at or before its least corner to the first at or after its
      // greatest one. This is how the game finds the size, since the file
      // does not say it.
      if ((info.flags & BspTextureInfo::special) == 0)
      {
        const auto first_s = static_cast<std::int64_t>(std::floor(least_s / units_for_each_sample));
        const auto first_t = static_cast<std::int64_t>(std::floor(least_t / units_for_each_sample));
        const auto last_s = static_cast<std::int64_t>(std::ceil(most_s / units_for_each_sample));
        const auto last_t = static_cast<std::int64_t>(std::ceil(most_t / units_for_each_sample));
        const std::int64_t width = last_s - first_s + 1;
        const std::int64_t height = last_t - first_t + 1;
        made.lightmap_width = static_cast<std::uint32_t>(width);
        made.lightmap_height = static_cast<std::uint32_t>(height);

        // the first sample lies on the first line, and its middle is half a
        // sample inside the lightmap
        for (std::size_t corner = 0; corner < on_texture.size(); corner++)
        {
          const auto [s, t] = on_texture[corner];
          BspMeshVertex &made_vertex = mesh.vertices[made.first_vertex + corner];
          made_vertex.lightmap_u = static_cast<float>(
            (s - static_cast<double>(first_s * units_for_each_sample) + units_for_each_sample / 2.0) /
            static_cast<double>(width * units_for_each_sample));
          made_vertex.lightmap_v = static_cast<float>(
            (t - static_cast<double>(first_t * units_for_each_sample) + units_for_each_sample / 2.0) /
            static_cast<double>(height * units_for_each_sample));
        }

        if (face.light_offset != -1)
        {
          const std::uint64_t size =
            static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height) * made.CountLightStyles();
          if (face.light_offset < 0 ||
            static_cast<std::uint64_t>(face.light_offset) > file.lighting.size() ||
            size > file.lighting.size() - static_cast<std::uint64_t>(face.light_offset))
          {
            return refuse(error, face_index, "has " + std::to_string(made.CountLightStyles()) + " lightmaps of " +
              std::to_string(width) + " by " + std::to_string(height) + " samples from byte " +
              std::to_string(face.light_offset) + " of the lighting, which has " +
              std::to_string(file.lighting.size()));
          }

          const auto start = file.lighting.begin() + face.light_offset;
          made.lightmap.assign(start, start + static_cast<std::ptrdiff_t>(size));
        }
      }

      // a fan around the first corner: (0, 1, 2), (0, 2, 3), and so on
      BspMeshGroup &group = groups[info.texture];
      group.texture = info.texture;
      group.is_sky = made.is_sky;
      group.is_liquid = made.is_liquid;
      for (std::uint32_t corner = 1; corner + 1 < made.vertex_count; corner++)
      {
        group.indices.push_back(made.first_vertex);
        group.indices.push_back(made.first_vertex + corner);
        group.indices.push_back(made.first_vertex + corner + 1);
      }

      mesh.faces.push_back(std::move(made));
    }

    mesh.groups.reserve(groups.size());
    for (auto &[texture, group] : groups) { mesh.groups.push_back(std::move(group)); }

    *this = std::move(mesh);
    return true;
  }
} // quake
