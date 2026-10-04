#include "bsp-light-point.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace quake
{
  // Helpers of BspLightPoint: a place counted with more digits than a level
  // has, the faces and forks of a level read as it keeps them, and the way
  // down the tree to the first face that holds light back.
  namespace
  {
    /// How many units of the level lie between two samples of a lightmap.
    constexpr double units_for_each_sample = 16.0;

    /// The largest number a corner may have on its texture. It is far more
    /// than any level has, and keeps the sizes of lightmaps inside what a
    /// whole number holds.
    constexpr double largest_extent = 1.0e9;

    /// A place on the way down, counted with more digits than the file has,
    /// so that cutting the way at plane after plane does not move it aside.
    struct Place
    {
      double x = 0.0;
      double y = 0.0;
      double z = 0.0;
    };

    /// What was met on the way down: which face, where, and where that is on
    /// its lightmap, counted in samples from its first one.
    struct Hit
    {
      std::size_t face = 0;
      Place place;
      double column = 0.0;
      double row = 0.0;
    };

    bool refuse(std::string &error, std::string reason)
    {
      error = std::move(reason);
      return false;
    }

    std::string names_outside(
      const std::string &who, const std::string &what, const std::int64_t number, const std::size_t count)
    {
      return who + " names " + what + " " + std::to_string(number) + ", and there are " + std::to_string(count);
    }

    /// `place . axis + offset`, counted with more digits than the file has,
    /// as the tools that made the lightmaps count it.
    double along(const Place &place, const BspVector &axis, const float offset)
    {
      return place.x * static_cast<double>(axis.x) +
        place.y * static_cast<double>(axis.y) +
        place.z * static_cast<double>(axis.z) +
        static_cast<double>(offset);
    }

    /// Reads a face of the level: the directions of its texture, how large
    /// its lightmap is, and, when it has one, where that starts. The size is
    /// not in the file. It follows from how far the corners of the face
    /// reach on its texture, as `BspMesh` finds it too.
    bool read_face(
      const BspFile &file,
      const std::size_t index,
      const std::size_t bytes_of_lighting,
      BspLightFace &made,
      std::string &error)
    {
      const std::string who = "face " + std::to_string(index);
      const BspFace &face = file.faces[index];

      if (face.texture_info < 0 || static_cast<std::size_t>(face.texture_info) >= file.texture_infos.size())
      {
        return refuse(error, names_outside(who, "texture info", face.texture_info, file.texture_infos.size()));
      }
      const BspTextureInfo &info = file.texture_infos[face.texture_info];

      made = BspLightFace();
      // the sky and the liquids carry no light and are looked through
      if ((info.flags & BspTextureInfo::special) != 0) { return true; }

      if (face.edge_count < 3)
      {
        return refuse(error, who + " has " + std::to_string(face.edge_count) + " corners, fewer than three");
      }
      if (face.first_edge < 0 ||
        static_cast<std::size_t>(face.first_edge) > file.face_edges.size() ||
        static_cast<std::size_t>(face.edge_count) > file.face_edges.size() - static_cast<std::size_t>(face.first_edge))
      {
        return refuse(error, who + " names edges of faces that are not there");
      }

      double least_s = 0.0;
      double least_t = 0.0;
      double most_s = 0.0;
      double most_t = 0.0;
      for (std::size_t corner = 0; corner < static_cast<std::size_t>(face.edge_count); corner++)
      {
        // an edge of a face is an edge walked forwards, from its first
        // vertex, or, when negative, backwards, from its second
        const std::int64_t face_edge = file.face_edges[static_cast<std::size_t>(face.first_edge) + corner];
        const std::int64_t edge = face_edge < 0 ? -face_edge : face_edge;
        if (static_cast<std::uint64_t>(edge) >= file.edges.size())
        {
          return refuse(error, names_outside(who, "edge", edge, file.edges.size()));
        }
        const std::uint32_t vertex = file.edges[static_cast<std::size_t>(edge)].vertices[face_edge < 0 ? 1 : 0];
        if (vertex >= file.vertices.size())
        {
          return refuse(error, names_outside(who, "vertex", vertex, file.vertices.size()));
        }

        const BspVector &position = file.vertices[vertex];
        const Place place{position.x, position.y, position.z};
        const double s = along(place, info.s_axis, info.s_offset);
        const double t = along(place, info.t_axis, info.t_offset);
        if (!(std::abs(s) <= largest_extent) || !(std::abs(t) <= largest_extent))
        {
          return refuse(error, who + " has a corner that lies nowhere on its texture");
        }

        if (corner == 0 || s < least_s) { least_s = s; }
        if (corner == 0 || s > most_s) { most_s = s; }
        if (corner == 0 || t < least_t) { least_t = t; }
        if (corner == 0 || t > most_t) { most_t = t; }
      }

      // a sample on every line of sixteen, from the last line at or before
      // the least corner to the first at or after the greatest one
      const double first_s = std::floor(least_s / units_for_each_sample);
      const double first_t = std::floor(least_t / units_for_each_sample);
      const auto width = static_cast<std::uint64_t>(std::ceil(most_s / units_for_each_sample) - first_s) + 1;
      const auto height = static_cast<std::uint64_t>(std::ceil(most_t / units_for_each_sample) - first_t) + 1;

      std::uint32_t style_count = 0;
      while (style_count < face.styles.size() && face.styles[style_count] != BspFace::no_style) { style_count++; }

      made.is_met = true;
      made.s_axis = info.s_axis;
      made.s_offset = info.s_offset;
      made.t_axis = info.t_axis;
      made.t_offset = info.t_offset;
      made.first_s = first_s * units_for_each_sample;
      made.first_t = first_t * units_for_each_sample;
      made.width = static_cast<std::uint32_t>(width);
      made.height = static_cast<std::uint32_t>(height);

      // a face no light reaches has no lightmap, and neither has one that
      // names no style: there is no sample to read
      if (face.light_offset == -1 || style_count == 0) { return true; }

      const std::uint64_t size = width * height * style_count;
      if (face.light_offset < 0 ||
        static_cast<std::uint64_t>(face.light_offset) > bytes_of_lighting ||
        size > bytes_of_lighting - static_cast<std::uint64_t>(face.light_offset))
      {
        return refuse(error, who + " has " + std::to_string(style_count) + " lightmaps of " +
          std::to_string(width) + " by " + std::to_string(height) + " samples from byte " +
          std::to_string(face.light_offset) + " of the lighting, which has " + std::to_string(bytes_of_lighting));
      }

      made.is_lit = true;
      made.styles = face.styles;
      made.style_count = style_count;
      made.first_sample = static_cast<std::size_t>(face.light_offset);
      return true;
    }

    /// A child of a node of the level as a fork keeps it: the number of a
    /// node, or the one number that stands for every leaf.
    bool read_child(
      const BspFile &file, const std::size_t node, const std::int32_t child, std::int32_t &made, std::string &error)
    {
      // the reader of levels hands a child over as it is: not negative for
      // a node, negative for a leaf
      if (child < 0)
      {
        made = BspLightNode::leaf;
        return true;
      }
      if (static_cast<std::size_t>(child) >= file.nodes.size())
      {
        return refuse(error, names_outside("node " + std::to_string(node), "node", child, file.nodes.size()));
      }
      made = child;
      return true;
    }

    /// How far a place is in front of the plane of a fork, negative when it
    /// is behind.
    double distance_in_front(const BspLightNode &node, const Place &place)
    {
      return place.x * static_cast<double>(node.normal.x) +
        place.y * static_cast<double>(node.normal.y) +
        place.z * static_cast<double>(node.normal.z) -
        static_cast<double>(node.distance);
    }

    /// Whether a place on the plane of a face lies on its lightmap, and
    /// where. As in the game, the whole rectangle of the lightmap counts,
    /// which reaches a little past the corners of the face.
    bool lies_on(const BspLightFace &face, const Place &place, const bool only_lit, Hit &hit)
    {
      if (!face.is_met || (only_lit && !face.is_lit)) { return false; }

      const double column = (along(place, face.s_axis, face.s_offset) - face.first_s) / units_for_each_sample;
      const double row = (along(place, face.t_axis, face.t_offset) - face.first_t) / units_for_each_sample;
      // written so that a place that is no number lies on nothing
      if (!(column >= 0.0 && column <= static_cast<double>(face.width - 1))) { return false; }
      if (!(row >= 0.0 && row <= static_cast<double>(face.height - 1))) { return false; }

      hit.place = place;
      hit.column = column;
      hit.row = row;
      return true;
    }

    /// Goes from `start` to `end` down the tree, from the fork or leaf `at`,
    /// and finds the first face the way crosses that holds light back, or,
    /// when `only_lit`, the first that has a lightmap. False when it crosses
    /// none.
    ///
    /// The way is cut at the plane of a fork: first the half on the side it
    /// starts on is looked at, then the faces on the plane itself, then the
    /// other half. It calls itself for the first half and goes on with the
    /// other in its own loop, so it is never deeper in calls than the tree
    /// is in forks.
    bool find(
      const std::span<const BspLightNode> nodes,
      const std::span<const BspLightFace> faces,
      std::int32_t at,
      Place start,
      const Place &end,
      const bool only_lit,
      Hit &hit)
    {
      while (at >= 0)
      {
        const BspLightNode &node = nodes[static_cast<std::size_t>(at)];
        const double front = distance_in_front(node, start);
        const double back = distance_in_front(node, end);
        const std::size_t side = front < 0.0 ? 1 : 0;

        // the way lies on one side whole
        if ((back < 0.0) == (front < 0.0))
        {
          at = node.children[side];
          continue;
        }

        // the two lie on different sides, so they are not as far
        const double part = front / (front - back);
        const Place middle{
          start.x + (end.x - start.x) * part,
          start.y + (end.y - start.y) * part,
          start.z + (end.z - start.z) * part,
        };

        if (find(nodes, faces, node.children[side], start, middle, only_lit, hit)) { return true; }

        // Of the faces on the plane, one with a lightmap comes before one
        // without: the rectangles of two faces may both hold the place, and
        // the dark one is then the one the place does not truly lie on.
        for (const bool lit : {true, false})
        {
          if (!lit && only_lit) { break; }
          for (std::size_t i = 0; i < node.face_count; i++)
          {
            const std::size_t face = node.first_face + i;
            if (faces[face].is_lit == lit && lies_on(faces[face], middle, only_lit, hit))
            {
              hit.face = face;
              return true;
            }
          }
        }

        at = node.children[1 - side];
        start = middle;
      }
      return false;
    }
  }

  bool BspLightPoint::Build(const BspFile &file, std::string &error, const std::span<const std::uint8_t> coloured)
  {
    if (file.models.empty()) { return refuse(error, "the level has no model, so it has no world"); }

    if (!coloured.empty() && coloured.size() != file.lighting.size() * 3)
    {
      return refuse(error, "the coloured light has " + std::to_string(coloured.size()) +
        " bytes, and three for each of the " + std::to_string(file.lighting.size()) +
        " of the lighting are " + std::to_string(file.lighting.size() * 3));
    }

    BspLightPoint made;
    made._has_light = !file.lighting.empty();
    made._faces.resize(file.faces.size());

    // A model that is a single leaf names it as a negative number, and has
    // no fork to look at.
    const std::int32_t head = file.models[0].head_nodes[0];
    if (head >= 0)
    {
      if (static_cast<std::size_t>(head) >= file.nodes.size())
      {
        return refuse(error, names_outside("model 0", "node", head, file.nodes.size()));
      }
      made._head = head;
      made._nodes.resize(file.nodes.size());

      // The forks the world reaches, each with how deep it lies. A fork
      // that is reached twice is refused, since a tree that loops would
      // never be walked to its end.
      std::vector<bool> is_reached(file.nodes.size(), false);
      std::vector<bool> is_face_read(file.faces.size(), false);
      std::vector<std::pair<std::size_t, std::size_t>> to_read;
      to_read.emplace_back(static_cast<std::size_t>(head), 1);
      is_reached[static_cast<std::size_t>(head)] = true;

      while (!to_read.empty())
      {
        const auto [index, depth] = to_read.back();
        to_read.pop_back();
        const std::string who = "node " + std::to_string(index);
        const BspNode &node = file.nodes[index];

        if (depth > deepest_tree)
        {
          return refuse(error, "the tree of the world is deeper than " + std::to_string(deepest_tree) + " nodes");
        }
        if (node.plane < 0 || static_cast<std::size_t>(node.plane) >= file.planes.size())
        {
          return refuse(error, names_outside(who, "plane", node.plane, file.planes.size()));
        }
        if (node.first_face > file.faces.size() || node.face_count > file.faces.size() - node.first_face)
        {
          return refuse(error, who + " names " + std::to_string(node.face_count) + " faces from " +
            std::to_string(node.first_face) + ", and there are " + std::to_string(file.faces.size()));
        }

        BspLightNode &fork = made._nodes[index];
        fork.normal = file.planes[static_cast<std::size_t>(node.plane)].normal;
        fork.distance = file.planes[static_cast<std::size_t>(node.plane)].distance;
        fork.first_face = node.first_face;
        fork.face_count = node.face_count;

        for (std::size_t i = 0; i < node.face_count; i++)
        {
          const std::size_t face = static_cast<std::size_t>(node.first_face) + i;
          if (is_face_read[face]) { continue; }
          is_face_read[face] = true;
          if (!read_face(file, face, file.lighting.size(), made._faces[face], error)) { return false; }
        }

        for (std::size_t side = 0; side < 2; side++)
        {
          if (!read_child(file, index, node.children[side], fork.children[side], error)) { return false; }
          if (fork.children[side] < 0) { continue; }

          const auto child = static_cast<std::size_t>(fork.children[side]);
          if (is_reached[child])
          {
            return refuse(error, who + " names node " + std::to_string(child) + ", which is reached twice");
          }
          is_reached[child] = true;
          to_read.emplace_back(child, depth + 1);
        }
      }
    }

    if (coloured.empty())
    {
      made._samples = file.lighting;
      made._bytes_for_each_sample = 1;
    }
    else
    {
      made._samples.assign(coloured.begin(), coloured.end());
      made._bytes_for_each_sample = 3;
    }

    *this = std::move(made);
    return true;
  }

  BspLightSample BspLightPoint::Sample(
    const BspVector &point, const std::span<const float> styles, const BspLightFilter filter) const
  {
    BspLightSample sample;
    if (!_has_light)
    {
      sample.red = fully_bright;
      sample.green = fully_bright;
      sample.blue = fully_bright;
      return sample;
    }

    const Place start{point.x, point.y, point.z};
    const Place end{start.x, start.y, start.z - static_cast<double>(deepest_look)};
    Hit hit;
    if (!find(_nodes, _faces, _head, start, end, false, hit)) { return sample; }

    // A face without a lightmap may be a sliver, or one whose rectangle
    // only reaches over the place, with the floor that is seen right under
    // it. So a face with light a little further down is taken in its place.
    if (!_faces[hit.face].is_lit)
    {
      const Place a_little_further{hit.place.x, hit.place.y, hit.place.z - static_cast<double>(dark_face_slack)};
      Hit lit_hit;
      if (find(_nodes, _faces, _head, start, a_little_further, true, lit_hit)) { hit = lit_hit; }
    }

    const BspLightFace &face = _faces[hit.face];
    sample.is_found = true;
    sample.place = {
      static_cast<float>(hit.place.x), static_cast<float>(hit.place.y), static_cast<float>(hit.place.z)};
    sample.face = static_cast<std::uint32_t>(hit.face);

    // a face no light reaches is dark
    if (!face.is_lit) { return sample; }

    // The samples around the spot and how much each counts. The spot lies
    // on the lightmap, so the first of each pair is a sample that is there;
    // the second is kept from going past the last.
    const auto column = static_cast<std::uint32_t>(hit.column);
    const auto row = static_cast<std::uint32_t>(hit.row);
    const std::uint32_t next_column = std::min(column + 1, face.width - 1);
    const std::uint32_t next_row = std::min(row + 1, face.height - 1);
    double right = 0.0;
    double down = 0.0;
    if (filter == BspLightFilter::Bilinear)
    {
      right = hit.column - static_cast<double>(column);
      down = hit.row - static_cast<double>(row);
    }

    const std::size_t samples_of_a_style = static_cast<std::size_t>(face.width) * face.height;
    std::array<double, 3> light{};
    for (std::uint32_t i = 0; i < face.style_count; i++)
    {
      const std::uint8_t style = face.styles[i];
      const double brightness = style < styles.size() ? static_cast<double>(styles[style]) : 1.0;
      const std::size_t first = face.first_sample + samples_of_a_style * i;

      for (std::size_t colour = 0; colour < 3; colour++)
      {
        // white light has one byte, which is all three colours
        const std::size_t byte = _bytes_for_each_sample == 3 ? colour : 0;
        const auto read = [&](const std::uint32_t at_column, const std::uint32_t at_row)
        {
          const std::size_t at = first + static_cast<std::size_t>(at_row) * face.width + at_column;
          return static_cast<double>(_samples[at * _bytes_for_each_sample + byte]);
        };

        const double above = read(column, row) * (1.0 - right) + read(next_column, row) * right;
        const double below = read(column, next_row) * (1.0 - right) + read(next_column, next_row) * right;
        light[colour] += (above * (1.0 - down) + below * down) * brightness;
      }
    }

    sample.red = static_cast<float>(light[0]);
    sample.green = static_cast<float>(light[1]);
    sample.blue = static_cast<float>(light[2]);
    return sample;
  }
} // quake
