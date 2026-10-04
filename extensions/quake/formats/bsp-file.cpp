#include "bsp-file.hpp"

#include <utility>

#include "byte-reader.hpp"

namespace quake
{
  // Helpers of BspFile: what each lump is called and how large its entries
  // are, a reader for each kind of entry, and the checks of what the entries
  // name.
  namespace
  {
    /// What a lump is called in an error, and how many bytes one of its
    /// entries takes in version 29, in `BSP2`, and in `2PSB`. A lump of
    /// text or of plain bytes has entries of one.
    struct LumpShape
    {
      const char *name;
      std::array<std::size_t, 3> entry_sizes;

      [[nodiscard]] constexpr std::size_t GetEntrySize(const BspFormat format) const
      {
        return entry_sizes[static_cast<std::size_t>(format)];
      }
    };

    constexpr std::array<LumpShape, BspFile::lump_count> lump_shapes = {{
      {"entities", {1, 1, 1}},
      {"planes", {20, 20, 20}},
      {"textures", {1, 1, 1}},
      {"vertices", {12, 12, 12}},
      {"visibility", {1, 1, 1}},
      {"nodes", {24, 44, 32}},
      {"texture infos", {40, 40, 40}},
      {"faces", {20, 28, 28}},
      {"lighting", {1, 1, 1}},
      {"clip nodes", {8, 12, 12}},
      {"leaves", {28, 44, 32}},
      {"faces of leaves", {2, 4, 4}},
      {"edges", {4, 8, 8}},
      {"edges of faces", {4, 4, 4}},
      {"models", {64, 64, 64}},
    }};

    /// Whether the form keeps in 32 bits what version 29 keeps in 16.
    bool is_wide(const BspFormat format)
    {
      return format != BspFormat::Version29;
    }

    bool refuse(std::string &error, std::string reason)
    {
      error = std::move(reason);
      return false;
    }

    /// Whether `value` is the number of one of `count` entries.
    bool is_inside(const std::int64_t value, const std::size_t count)
    {
      return value >= 0 && static_cast<std::uint64_t>(value) < count;
    }

    /// Whether `count` entries from `first` on are all among `size` entries.
    bool is_range_inside(const std::int64_t first, const std::int64_t count, const std::size_t size)
    {
      return first >= 0 && count >= 0 &&
        static_cast<std::uint64_t>(first) <= size &&
        static_cast<std::uint64_t>(count) <= size - static_cast<std::uint64_t>(first);
    }

    /// The sentence that refuses an entry naming something that is not
    /// there, such as "face 3 names plane 7, and there are 2".
    std::string names_outside(
      const char *owner,
      const std::size_t index,
      const char *thing,
      const std::int64_t value,
      const std::size_t count)
    {
      return std::string(owner) + " " + std::to_string(index) + " names " + thing + " " + std::to_string(value) +
        ", and there are " + std::to_string(count);
    }

    /// The same for an entry naming several in a row, such as "face 3 names
    /// 4 edges of faces from 5, and there are 6".
    std::string names_range_outside(
      const char *owner,
      const std::size_t index,
      const char *things,
      const std::int64_t first,
      const std::int64_t count,
      const std::size_t size)
    {
      return std::string(owner) + " " + std::to_string(index) + " names " + std::to_string(count) + " " + things +
        " from " + std::to_string(first) + ", and there are " + std::to_string(size);
    }

    /// Every entry of a lump, each read by `read_entry`. The size of the
    /// lump is a multiple of `entry_size` by the time this is asked.
    template<typename T, typename ReadEntry>
    std::vector<T> read_entries(
      const std::span<const std::uint8_t> bytes,
      const std::size_t entry_size,
      ReadEntry read_entry)
    {
      const std::size_t count = bytes.size() / entry_size;
      ByteReader reader(bytes);

      std::vector<T> entries;
      entries.reserve(count);
      for (std::size_t i = 0; i < count; i++) { entries.push_back(read_entry(reader)); }
      return entries;
    }

    BspVector read_vector(ByteReader &reader)
    {
      BspVector vector;
      vector.x = reader.ReadF32();
      vector.y = reader.ReadF32();
      vector.z = reader.ReadF32();
      return vector;
    }

    /// A corner of the box of a node or a leaf, which only `BSP2` keeps as
    /// numbers with a fraction.
    std::array<float, 3> read_corner(ByteReader &reader, const BspFormat format)
    {
      std::array<float, 3> corner{};
      for (float &number : corner)
      {
        number = format == BspFormat::Bsp2 ? reader.ReadF32() : static_cast<float>(reader.ReadI16());
      }
      return corner;
    }

    /// A number that version 29 keeps in 16 bits without a sign.
    std::uint32_t read_count(ByteReader &reader, const BspFormat format)
    {
      return is_wide(format) ? reader.ReadU32() : reader.ReadU16();
    }

    /// A child of a node or a clip node, of which the lump has `count`.
    ///
    /// Version 29 keeps it in 16 bits. A level with more forks than a
    /// signed number of those counts keeps the numbers of the forks above
    /// that as negative ones, as the compilers of today write them and the
    /// source ports read them: a child is a fork when, read without a sign,
    /// it is the number of one that is there, and only otherwise the
    /// negative number it looks like.
    std::int32_t read_child(ByteReader &reader, const BspFormat format, const std::size_t count)
    {
      if (is_wide(format)) { return reader.ReadI32(); }

      const std::uint16_t child = reader.ReadU16();
      if (child < count) { return child; }
      return static_cast<std::int16_t>(child);
    }

    BspPlane read_plane(ByteReader &reader)
    {
      BspPlane plane;
      plane.normal = read_vector(reader);
      plane.distance = reader.ReadF32();
      plane.type = reader.ReadI32();
      return plane;
    }

    BspNode read_node(ByteReader &reader, const BspFormat format, const std::size_t count)
    {
      BspNode node;
      node.plane = reader.ReadI32();
      node.children[0] = read_child(reader, format, count);
      node.children[1] = read_child(reader, format, count);
      node.mins = read_corner(reader, format);
      node.maxs = read_corner(reader, format);
      node.first_face = read_count(reader, format);
      node.face_count = read_count(reader, format);
      return node;
    }

    BspTextureInfo read_texture_info(ByteReader &reader)
    {
      BspTextureInfo info;
      info.s_axis = read_vector(reader);
      info.s_offset = reader.ReadF32();
      info.t_axis = read_vector(reader);
      info.t_offset = reader.ReadF32();
      info.texture = reader.ReadI32();
      info.flags = reader.ReadI32();
      return info;
    }

    BspFace read_face(ByteReader &reader, const BspFormat format)
    {
      BspFace face;
      if (is_wide(format))
      {
        face.plane = reader.ReadI32();
        face.side = reader.ReadI32();
        face.first_edge = reader.ReadI32();
        face.edge_count = reader.ReadI32();
        face.texture_info = reader.ReadI32();
      }
      else
      {
        face.plane = reader.ReadU16();
        face.side = reader.ReadI16();
        face.first_edge = reader.ReadI32();
        face.edge_count = reader.ReadU16();
        face.texture_info = reader.ReadU16();
      }
      for (std::uint8_t &style : face.styles) { style = reader.ReadU8(); }
      face.light_offset = reader.ReadI32();
      return face;
    }

    BspClipNode read_clip_node(ByteReader &reader, const BspFormat format, const std::size_t count)
    {
      BspClipNode node;
      node.plane = reader.ReadI32();
      node.children[0] = read_child(reader, format, count);
      node.children[1] = read_child(reader, format, count);
      return node;
    }

    BspLeaf read_leaf(ByteReader &reader, const BspFormat format)
    {
      BspLeaf leaf;
      leaf.contents = reader.ReadI32();
      leaf.visibility_offset = reader.ReadI32();
      leaf.mins = read_corner(reader, format);
      leaf.maxs = read_corner(reader, format);
      leaf.first_leaf_face = read_count(reader, format);
      leaf.leaf_face_count = read_count(reader, format);
      for (std::uint8_t &level : leaf.ambient_levels) { level = reader.ReadU8(); }
      return leaf;
    }

    BspEdge read_edge(ByteReader &reader, const BspFormat format)
    {
      BspEdge edge;
      edge.vertices[0] = read_count(reader, format);
      edge.vertices[1] = read_count(reader, format);
      return edge;
    }

    BspModel read_model(ByteReader &reader)
    {
      BspModel model;
      model.mins = read_vector(reader);
      model.maxs = read_vector(reader);
      model.origin = read_vector(reader);
      for (std::int32_t &head_node : model.head_nodes) { head_node = reader.ReadI32(); }
      model.leaf_count = reader.ReadI32();
      model.first_face = reader.ReadI32();
      model.face_count = reader.ReadI32();
      return model;
    }

    /// The textures of the lump of textures: how many there are, where each
    /// starts in the lump, and then the textures themselves.
    bool read_textures(
      const std::span<const std::uint8_t> bytes,
      std::vector<std::optional<MipTexture>> &textures,
      std::string &error)
    {
      // a level without textures has an empty lump
      if (bytes.empty()) { return true; }

      if (bytes.size() < 4)
      {
        return refuse(error, "the lump of textures has " + std::to_string(bytes.size()) +
          " bytes, fewer than the 4 of its count");
      }

      ByteReader reader(bytes);
      const std::int32_t count = reader.ReadI32();
      if (count < 0 || static_cast<std::size_t>(count) > (bytes.size() - 4) / 4)
      {
        return refuse(error, "the lump of textures says it has " + std::to_string(count) +
          " textures, and has " + std::to_string(bytes.size()) + " bytes");
      }

      textures.reserve(static_cast<std::size_t>(count));
      for (std::int32_t i = 0; i < count; i++)
      {
        const std::int32_t offset = reader.ReadI32();

        // a texture that is named but not carried, which the game draws as
        // a chequered stand-in
        if (offset == -1)
        {
          textures.emplace_back();
          continue;
        }

        if (offset < 0 || static_cast<std::size_t>(offset) > bytes.size())
        {
          return refuse(error, "texture " + std::to_string(i) + " starts at byte " + std::to_string(offset) +
            " of the lump of textures, which has " + std::to_string(bytes.size()));
        }

        MipTexture texture;
        if (std::string reason; !texture.Read(bytes.subspan(static_cast<std::size_t>(offset)), reason))
        {
          return refuse(error, "texture " + std::to_string(i) + ": " + reason);
        }
        textures.emplace_back(std::move(texture));
      }
      return true;
    }

    /// Whether everything an entry names is there. Says the first that is
    /// not.
    bool check_what_entries_name(const BspFile &file, std::string &error)
    {
      for (std::size_t i = 0; i < file.texture_infos.size(); i++)
      {
        const BspTextureInfo &info = file.texture_infos[i];
        if (!is_inside(info.texture, file.textures.size()))
        {
          return refuse(error, names_outside("texture info", i, "texture", info.texture, file.textures.size()));
        }
      }

      for (std::size_t i = 0; i < file.edges.size(); i++)
      {
        for (const std::uint32_t vertex : file.edges[i].vertices)
        {
          if (!is_inside(vertex, file.vertices.size()))
          {
            return refuse(error, names_outside("edge", i, "vertex", vertex, file.vertices.size()));
          }
        }
      }

      for (std::size_t i = 0; i < file.face_edges.size(); i++)
      {
        // the edge is walked backwards when the entry is negative. The
        // lowest number has no positive form, and is too large an edge anyway
        const std::int64_t edge = file.face_edges[i] < 0
          ? -static_cast<std::int64_t>(file.face_edges[i])
          : file.face_edges[i];
        if (!is_inside(edge, file.edges.size()))
        {
          return refuse(error, names_outside("edge of faces", i, "edge", edge, file.edges.size()));
        }
      }

      for (std::size_t i = 0; i < file.faces.size(); i++)
      {
        const BspFace &face = file.faces[i];
        if (!is_inside(face.plane, file.planes.size()))
        {
          return refuse(error, names_outside("face", i, "plane", face.plane, file.planes.size()));
        }
        if (!is_inside(face.texture_info, file.texture_infos.size()))
        {
          return refuse(error,
            names_outside("face", i, "texture info", face.texture_info, file.texture_infos.size()));
        }
        if (!is_range_inside(face.first_edge, face.edge_count, file.face_edges.size()))
        {
          return refuse(error, names_range_outside(
            "face", i, "edges of faces", face.first_edge, face.edge_count, file.face_edges.size()));
        }
        // how many bytes the lightmap takes follows from the size of the
        // face, which is for what is made of the level to work out
        if (face.light_offset != -1 && !is_inside(face.light_offset, file.lighting.size()))
        {
          return refuse(error,
            names_outside("face", i, "byte of lighting", face.light_offset, file.lighting.size()));
        }
      }

      for (std::size_t i = 0; i < file.leaf_faces.size(); i++)
      {
        if (!is_inside(file.leaf_faces[i], file.faces.size()))
        {
          return refuse(error, names_outside("face of leaves", i, "face", file.leaf_faces[i], file.faces.size()));
        }
      }

      for (std::size_t i = 0; i < file.leaves.size(); i++)
      {
        const BspLeaf &leaf = file.leaves[i];
        if (!is_range_inside(leaf.first_leaf_face, leaf.leaf_face_count, file.leaf_faces.size()))
        {
          return refuse(error, names_range_outside(
            "leaf", i, "faces of leaves", leaf.first_leaf_face, leaf.leaf_face_count, file.leaf_faces.size()));
        }
        // A level without visibility, as the small ones that are items, has
        // leaves that name its first byte all the same. There is nothing to
        // look up then, and whoever reads the visibility sees that it is empty.
        if (leaf.visibility_offset != -1 && !file.visibility.empty() &&
            !is_inside(leaf.visibility_offset, file.visibility.size()))
        {
          return refuse(error,
            names_outside("leaf", i, "byte of visibility", leaf.visibility_offset, file.visibility.size()));
        }
      }

      for (std::size_t i = 0; i < file.nodes.size(); i++)
      {
        const BspNode &node = file.nodes[i];
        if (!is_inside(node.plane, file.planes.size()))
        {
          return refuse(error, names_outside("node", i, "plane", node.plane, file.planes.size()));
        }
        for (const std::int32_t child : node.children)
        {
          if (child >= 0 && !is_inside(child, file.nodes.size()))
          {
            return refuse(error, names_outside("node", i, "node", child, file.nodes.size()));
          }
          // a negative child is a leaf, counted down from -1
          const std::int64_t leaf = -(static_cast<std::int64_t>(child) + 1);
          if (child < 0 && !is_inside(leaf, file.leaves.size()))
          {
            return refuse(error, names_outside("node", i, "leaf", leaf, file.leaves.size()));
          }
        }
        if (!is_range_inside(node.first_face, node.face_count, file.faces.size()))
        {
          return refuse(error,
            names_range_outside("node", i, "faces", node.first_face, node.face_count, file.faces.size()));
        }
      }

      for (std::size_t i = 0; i < file.clip_nodes.size(); i++)
      {
        const BspClipNode &node = file.clip_nodes[i];
        if (!is_inside(node.plane, file.planes.size()))
        {
          return refuse(error, names_outside("clip node", i, "plane", node.plane, file.planes.size()));
        }
        for (const std::int32_t child : node.children)
        {
          // a negative child is what fills the space, not an entry
          if (child >= 0 && !is_inside(child, file.clip_nodes.size()))
          {
            return refuse(error, names_outside("clip node", i, "clip node", child, file.clip_nodes.size()));
          }
        }
      }

      for (std::size_t i = 0; i < file.models.size(); i++)
      {
        const BspModel &model = file.models[i];
        if (!is_range_inside(model.first_face, model.face_count, file.faces.size()))
        {
          return refuse(error,
            names_range_outside("model", i, "faces", model.first_face, model.face_count, file.faces.size()));
        }

        // the tree it is drawn by starts at a node, or is a single leaf
        const std::int32_t head_node = model.head_nodes[0];
        if (head_node >= 0 && !is_inside(head_node, file.nodes.size()))
        {
          return refuse(error, names_outside("model", i, "node", head_node, file.nodes.size()));
        }
        if (head_node < 0 && !is_inside(-(static_cast<std::int64_t>(head_node) + 1), file.leaves.size()))
        {
          return refuse(error, names_outside(
            "model", i, "leaf", -(static_cast<std::int64_t>(head_node) + 1), file.leaves.size()));
        }

        // the two trees things collide with. The fourth tree is not used by
        // the game and is left as it is
        for (std::size_t hull = 1; hull <= 2; hull++)
        {
          const std::int32_t clip_node = model.head_nodes[hull];
          if (clip_node >= 0 && !is_inside(clip_node, file.clip_nodes.size()))
          {
            return refuse(error, names_outside("model", i, "clip node", clip_node, file.clip_nodes.size()));
          }
        }
      }

      return true;
    }
  }

  bool BspFile::Read(const std::span<const std::uint8_t> bytes, std::string &error)
  {
    if (bytes.size() < 4)
    {
      return refuse(error, "the file has " + std::to_string(bytes.size()) + " bytes, fewer than the 4 of its version");
    }

    ByteReader header(bytes);
    BspFile file;
    switch (const std::int32_t found = header.ReadI32())
    {
      case version:
        file.format = BspFormat::Version29;
        break;
      case bsp2_magic:
        file.format = BspFormat::Bsp2;
        break;
      case bsp2_rmq_magic:
        file.format = BspFormat::Bsp2Rmq;
        break;
      default:
        return refuse(error, "the file is version " + std::to_string(found) + ", and only version " +
          std::to_string(version) + " and the forms BSP2 and 2PSB are read");
    }
    const BspFormat format = file.format;

    if (bytes.size() < header_size)
    {
      return refuse(error, "the file has " + std::to_string(bytes.size()) + " bytes, fewer than the " +
        std::to_string(header_size) + " of its header");
    }

    std::array<std::span<const std::uint8_t>, lump_count> parts;
    for (std::size_t i = 0; i < lump_count; i++)
    {
      BspLump &lump = file.lumps[i];
      lump.offset = header.ReadI32();
      lump.size = header.ReadI32();

      if (lump.offset < 0 || lump.size < 0 ||
        static_cast<std::size_t>(lump.offset) > bytes.size() ||
        static_cast<std::size_t>(lump.size) > bytes.size() - static_cast<std::size_t>(lump.offset))
      {
        return refuse(error, std::string("the lump of ") + lump_shapes[i].name + ", " + std::to_string(lump.size) +
          " bytes from byte " + std::to_string(lump.offset) + ", lies outside the file of " +
          std::to_string(bytes.size()) + " bytes");
      }

      if (static_cast<std::size_t>(lump.size) % lump_shapes[i].GetEntrySize(format) != 0)
      {
        return refuse(error, std::string("the lump of ") + lump_shapes[i].name + " has " +
          std::to_string(lump.size) + " bytes, which is not a multiple of the " +
          std::to_string(lump_shapes[i].GetEntrySize(format)) + " of an entry");
      }

      parts[i] = bytes.subspan(static_cast<std::size_t>(lump.offset), static_cast<std::size_t>(lump.size));
    }

    const auto part = [&parts](const BspLumpKind kind) { return parts[static_cast<std::size_t>(kind)]; };
    const auto entry_size = [format](const BspLumpKind kind)
    {
      return lump_shapes[static_cast<std::size_t>(kind)].GetEntrySize(format);
    };
    const auto count_of = [&part, &entry_size](const BspLumpKind kind)
    {
      return part(kind).size() / entry_size(kind);
    };

    // the text ends at its zero, which the tools write after it
    for (const std::uint8_t letter : part(BspLumpKind::Entities))
    {
      if (letter == 0) { break; }
      file.entities.push_back(static_cast<char>(letter));
    }

    file.planes = read_entries<BspPlane>(part(BspLumpKind::Planes), entry_size(BspLumpKind::Planes), read_plane);
    file.vertices =
      read_entries<BspVector>(part(BspLumpKind::Vertices), entry_size(BspLumpKind::Vertices), read_vector);
    file.nodes = read_entries<BspNode>(
      part(BspLumpKind::Nodes), entry_size(BspLumpKind::Nodes),
      [format, count = count_of(BspLumpKind::Nodes)](ByteReader &reader)
      {
        return read_node(reader, format, count);
      });
    file.texture_infos = read_entries<BspTextureInfo>(
      part(BspLumpKind::TextureInfos), entry_size(BspLumpKind::TextureInfos), read_texture_info);
    file.faces = read_entries<BspFace>(
      part(BspLumpKind::Faces), entry_size(BspLumpKind::Faces),
      [format](ByteReader &reader) { return read_face(reader, format); });
    file.clip_nodes = read_entries<BspClipNode>(
      part(BspLumpKind::ClipNodes), entry_size(BspLumpKind::ClipNodes),
      [format, count = count_of(BspLumpKind::ClipNodes)](ByteReader &reader)
      {
        return read_clip_node(reader, format, count);
      });
    file.leaves = read_entries<BspLeaf>(
      part(BspLumpKind::Leaves), entry_size(BspLumpKind::Leaves),
      [format](ByteReader &reader) { return read_leaf(reader, format); });
    file.leaf_faces = read_entries<std::uint32_t>(
      part(BspLumpKind::LeafFaces), entry_size(BspLumpKind::LeafFaces),
      [format](ByteReader &reader) { return read_count(reader, format); });
    file.edges = read_entries<BspEdge>(
      part(BspLumpKind::Edges), entry_size(BspLumpKind::Edges),
      [format](ByteReader &reader) { return read_edge(reader, format); });
    file.face_edges = read_entries<std::int32_t>(
      part(BspLumpKind::FaceEdges), entry_size(BspLumpKind::FaceEdges),
      [](ByteReader &reader) { return reader.ReadI32(); });
    file.models = read_entries<BspModel>(part(BspLumpKind::Models), entry_size(BspLumpKind::Models), read_model);

    const auto visibility = part(BspLumpKind::Visibility);
    file.visibility.assign(visibility.begin(), visibility.end());
    const auto lighting = part(BspLumpKind::Lighting);
    file.lighting.assign(lighting.begin(), lighting.end());

    if (!read_textures(part(BspLumpKind::Textures), file.textures, error)) { return false; }
    if (!check_what_entries_name(file, error)) { return false; }

    *this = std::move(file);
    return true;
  }

  const BspLump &BspFile::GetLump(const BspLumpKind kind) const
  {
    return lumps[static_cast<std::size_t>(kind)];
  }
} // quake
