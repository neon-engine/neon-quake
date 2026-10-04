#include "bsp-file.hpp"

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "real-data.test.hpp"

namespace
{
  using quake::BspFile;
  using quake::BspFormat;
  using quake::BspLumpKind;
  using quake::RealData;
  using ::testing::ElementsAre;
  using ::testing::HasSubstr;

  using Bytes = std::vector<std::uint8_t>;

  void PutU8(Bytes &bytes, const std::uint8_t value)
  {
    bytes.push_back(value);
  }

  void PutU16(Bytes &bytes, const std::uint16_t value)
  {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
  }

  void PutI16(Bytes &bytes, const std::int16_t value)
  {
    PutU16(bytes, static_cast<std::uint16_t>(value));
  }

  void PutU32(Bytes &bytes, const std::uint32_t value)
  {
    for (int shift = 0; shift < 32; shift += 8) { bytes.push_back(static_cast<std::uint8_t>(value >> shift)); }
  }

  void PutI32(Bytes &bytes, const std::int32_t value)
  {
    PutU32(bytes, static_cast<std::uint32_t>(value));
  }

  void PutF32(Bytes &bytes, const float value)
  {
    std::uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    PutU32(bytes, bits);
  }

  void PutVector(Bytes &bytes, const float x, const float y, const float z)
  {
    PutF32(bytes, x);
    PutF32(bytes, y);
    PutF32(bytes, z);
  }

  void SetI16(Bytes &bytes, const std::size_t at, const std::int16_t value)
  {
    bytes[at] = static_cast<std::uint8_t>(value);
    bytes[at + 1] = static_cast<std::uint8_t>(static_cast<std::uint16_t>(value) >> 8);
  }

  void SetI32(Bytes &bytes, const std::size_t at, const std::int32_t value)
  {
    const auto bits = static_cast<std::uint32_t>(value);
    for (int i = 0; i < 4; i++) { bytes[at + i] = static_cast<std::uint8_t>(bits >> (8 * i)); }
  }

  /// The bytes of a texture of 16 by 16 pixels, all of colour `colour`, with
  /// its three smaller sizes.
  Bytes MakeTexture(const std::string &name, const std::uint8_t colour)
  {
    Bytes bytes;
    for (std::size_t i = 0; i < 16; i++) { PutU8(bytes, i < name.size() ? static_cast<std::uint8_t>(name[i]) : 0); }
    PutU32(bytes, 16);
    PutU32(bytes, 16);
    PutU32(bytes, 40);
    PutU32(bytes, 40 + 256);
    PutU32(bytes, 40 + 256 + 64);
    PutU32(bytes, 40 + 256 + 64 + 16);
    bytes.insert(bytes.end(), 256 + 64 + 16 + 4, colour);
    return bytes;
  }

  void SetF32(Bytes &bytes, const std::size_t at, const float value)
  {
    std::int32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    SetI32(bytes, at, bits);
  }

  /// Whether a form keeps in 32 bits what version 29 keeps in 16.
  bool IsWide(const BspFormat format)
  {
    return format != BspFormat::Version29;
  }

  /// A number that version 29 keeps in 16 bits and the wider forms in 32:
  /// a child, a count, or the number of an entry.
  void PutNumber(Bytes &bytes, const BspFormat format, const std::int32_t value)
  {
    if (IsWide(format)) { PutI32(bytes, value); }
    else { PutI16(bytes, static_cast<std::int16_t>(value)); }
  }

  /// A number of the box of a node or a leaf, which only `BSP2` keeps with
  /// a fraction.
  void PutBound(Bytes &bytes, const BspFormat format, const float value)
  {
    if (format == BspFormat::Bsp2) { PutF32(bytes, value); }
    else { PutI16(bytes, static_cast<std::int16_t>(value)); }
  }

  /// What the file of a form starts with.
  std::int32_t VersionOf(const BspFormat format)
  {
    switch (format)
    {
      case BspFormat::Bsp2: return BspFile::bsp2_magic;
      case BspFormat::Bsp2Rmq: return BspFile::bsp2_rmq_magic;
      default: return BspFile::version;
    }
  }

  /// A level taken apart into its lumps, so that a test can change one
  /// before the file is put together.
  struct Level
  {
    std::int32_t version = 29;
    std::array<Bytes, 15> lumps;

    Bytes &Lump(const BspLumpKind kind)
    {
      return lumps[static_cast<std::size_t>(kind)];
    }

    /// The file: the header, then the lumps one after the other.
    [[nodiscard]] Bytes ToBytes() const
    {
      Bytes bytes;
      PutI32(bytes, version);

      std::size_t offset = 4 + 15 * 8;
      for (const Bytes &lump : lumps)
      {
        PutI32(bytes, static_cast<std::int32_t>(offset));
        PutI32(bytes, static_cast<std::int32_t>(lump.size()));
        offset += lump.size();
      }
      for (const Bytes &lump : lumps) { bytes.insert(bytes.end(), lump.begin(), lump.end()); }
      return bytes;
    }
  };

  /// The smallest level that is whole: a floor of one face with four
  /// corners, 100 by 48 units, looking up. It has two textures, the second
  /// of which is named and not carried, one node with two leaves, one clip
  /// node, and one model.
  ///
  /// It is written in the form asked for, with the same numbers in each.
  Level MakeQuad(const BspFormat format = BspFormat::Version29)
  {
    Level level;
    level.version = VersionOf(format);

    const std::string entities = "{\n\"classname\" \"worldspawn\"\n}\n";
    Bytes &text = level.Lump(BspLumpKind::Entities);
    text.assign(entities.begin(), entities.end());
    text.push_back(0);

    Bytes &planes = level.Lump(BspLumpKind::Planes);
    PutVector(planes, 0.0f, 0.0f, 1.0f);
    PutF32(planes, 0.0f);
    PutI32(planes, 2);

    Bytes &textures = level.Lump(BspLumpKind::Textures);
    PutI32(textures, 2);
    PutI32(textures, 12);
    PutI32(textures, -1);
    const Bytes texture = MakeTexture("floor", 7);
    textures.insert(textures.end(), texture.begin(), texture.end());

    Bytes &vertices = level.Lump(BspLumpKind::Vertices);
    PutVector(vertices, 0.0f, 0.0f, 0.0f);
    PutVector(vertices, 0.0f, 48.0f, 0.0f);
    PutVector(vertices, 100.0f, 48.0f, 0.0f);
    PutVector(vertices, 100.0f, 0.0f, 0.0f);

    level.Lump(BspLumpKind::Visibility) = {0x01};

    Bytes &nodes = level.Lump(BspLumpKind::Nodes);
    PutI32(nodes, 0);
    PutNumber(nodes, format, -2);
    PutNumber(nodes, format, -1);
    for (const float number : {0.0f, 0.0f, -16.0f, 100.0f, 48.0f, 16.0f}) { PutBound(nodes, format, number); }
    PutNumber(nodes, format, 0);
    PutNumber(nodes, format, 1);

    Bytes &infos = level.Lump(BspLumpKind::TextureInfos);
    PutVector(infos, 1.0f, 0.0f, 0.0f);
    PutF32(infos, 8.0f);
    PutVector(infos, 0.0f, -1.0f, 0.0f);
    PutF32(infos, 0.0f);
    PutI32(infos, 0);
    PutI32(infos, 0);

    Bytes &faces = level.Lump(BspLumpKind::Faces);
    PutNumber(faces, format, 0);
    PutNumber(faces, format, 0);
    PutI32(faces, 0);
    PutNumber(faces, format, 4);
    PutNumber(faces, format, 0);
    for (const std::uint8_t style : {0, 255, 255, 255}) { PutU8(faces, style); }
    PutI32(faces, 4);

    // four bytes of nobody, then the 8 by 4 samples of the face
    Bytes &lighting = level.Lump(BspLumpKind::Lighting);
    lighting.assign(4, 0xaa);
    for (int i = 0; i < 32; i++) { PutU8(lighting, static_cast<std::uint8_t>(i)); }

    Bytes &clip_nodes = level.Lump(BspLumpKind::ClipNodes);
    PutI32(clip_nodes, 0);
    PutNumber(clip_nodes, format, -1);
    PutNumber(clip_nodes, format, -2);

    // leaf 0 is everything solid, leaf 1 is the room above the floor
    Bytes &leaves = level.Lump(BspLumpKind::Leaves);
    PutI32(leaves, -2);
    PutI32(leaves, -1);
    for (int i = 0; i < 6; i++) { PutBound(leaves, format, 0.0f); }
    PutNumber(leaves, format, 0);
    PutNumber(leaves, format, 0);
    for (int i = 0; i < 4; i++) { PutU8(leaves, 0); }
    PutI32(leaves, -1);
    PutI32(leaves, 0);
    for (const float number : {0.0f, 0.0f, 0.0f, 100.0f, 48.0f, 16.0f}) { PutBound(leaves, format, number); }
    PutNumber(leaves, format, 0);
    PutNumber(leaves, format, 1);
    for (const std::uint8_t level_of_sound : {10, 20, 30, 40}) { PutU8(leaves, level_of_sound); }

    PutNumber(level.Lump(BspLumpKind::LeafFaces), format, 0);

    // edge 0 is never used, since an edge of a face says its direction by its sign
    Bytes &edges = level.Lump(BspLumpKind::Edges);
    for (const std::int32_t vertex : {0, 0, 0, 1, 1, 2, 3, 2, 3, 0}) { PutNumber(edges, format, vertex); }

    Bytes &face_edges = level.Lump(BspLumpKind::FaceEdges);
    for (const std::int32_t edge : {1, 2, -3, 4}) { PutI32(face_edges, edge); }

    Bytes &models = level.Lump(BspLumpKind::Models);
    PutVector(models, 0.0f, 0.0f, -16.0f);
    PutVector(models, 100.0f, 48.0f, 16.0f);
    PutVector(models, 0.0f, 0.0f, 0.0f);
    for (int i = 0; i < 4; i++) { PutI32(models, 0); }
    PutI32(models, 1);
    PutI32(models, 0);
    PutI32(models, 1);

    return level;
  }

  /// What reading a level says is wrong with it. Empty when nothing is.
  std::string ReasonOfRefusal(const Level &level)
  {
    BspFile file;
    std::string error;
    if (file.Read(level.ToBytes(), error)) { return {}; }
    EXPECT_FALSE(error.empty());
    return error;
  }

  TEST(BspFileTest, ReadsTheHeaderAndEveryLump)
  {
    const Level level = MakeQuad();
    const Bytes bytes = level.ToBytes();

    BspFile file;
    std::string error;
    ASSERT_TRUE(file.Read(bytes, error)) << error;
    EXPECT_EQ(file.format, BspFormat::Version29);

    EXPECT_EQ(file.GetLump(BspLumpKind::Entities).offset, 124);
    EXPECT_EQ(file.GetLump(BspLumpKind::Entities).size, 30);
    EXPECT_EQ(file.GetLump(BspLumpKind::Planes).offset, 154);
    EXPECT_EQ(file.GetLump(BspLumpKind::Models).size, 64);

    EXPECT_EQ(file.entities, "{\n\"classname\" \"worldspawn\"\n}\n");

    ASSERT_EQ(file.planes.size(), 1u);
    EXPECT_EQ(file.planes[0].normal.z, 1.0f);
    EXPECT_EQ(file.planes[0].distance, 0.0f);
    EXPECT_EQ(file.planes[0].type, 2);

    ASSERT_EQ(file.vertices.size(), 4u);
    EXPECT_EQ(file.vertices[2].x, 100.0f);
    EXPECT_EQ(file.vertices[2].y, 48.0f);
    EXPECT_EQ(file.vertices[2].z, 0.0f);

    EXPECT_THAT(file.visibility, ElementsAre(0x01));

    ASSERT_EQ(file.nodes.size(), 1u);
    EXPECT_EQ(file.nodes[0].plane, 0);
    EXPECT_THAT(file.nodes[0].children, ElementsAre(-2, -1));
    EXPECT_THAT(file.nodes[0].mins, ElementsAre(0, 0, -16));
    EXPECT_THAT(file.nodes[0].maxs, ElementsAre(100, 48, 16));
    EXPECT_EQ(file.nodes[0].first_face, 0);
    EXPECT_EQ(file.nodes[0].face_count, 1);

    ASSERT_EQ(file.texture_infos.size(), 1u);
    EXPECT_EQ(file.texture_infos[0].s_axis.x, 1.0f);
    EXPECT_EQ(file.texture_infos[0].s_offset, 8.0f);
    EXPECT_EQ(file.texture_infos[0].t_axis.y, -1.0f);
    EXPECT_EQ(file.texture_infos[0].t_offset, 0.0f);
    EXPECT_EQ(file.texture_infos[0].texture, 0);
    EXPECT_EQ(file.texture_infos[0].flags, 0);

    ASSERT_EQ(file.faces.size(), 1u);
    EXPECT_EQ(file.faces[0].plane, 0);
    EXPECT_EQ(file.faces[0].side, 0);
    EXPECT_EQ(file.faces[0].first_edge, 0);
    EXPECT_EQ(file.faces[0].edge_count, 4);
    EXPECT_EQ(file.faces[0].texture_info, 0);
    EXPECT_THAT(file.faces[0].styles, ElementsAre(0, 255, 255, 255));
    EXPECT_EQ(file.faces[0].light_offset, 4);

    ASSERT_EQ(file.lighting.size(), 36u);
    EXPECT_EQ(file.lighting[0], 0xaa);
    EXPECT_EQ(file.lighting[35], 31);

    ASSERT_EQ(file.clip_nodes.size(), 1u);
    EXPECT_EQ(file.clip_nodes[0].plane, 0);
    EXPECT_THAT(file.clip_nodes[0].children, ElementsAre(-1, -2));

    ASSERT_EQ(file.leaves.size(), 2u);
    EXPECT_EQ(file.leaves[0].contents, -2);
    EXPECT_EQ(file.leaves[0].visibility_offset, -1);
    EXPECT_EQ(file.leaves[1].contents, -1);
    EXPECT_EQ(file.leaves[1].visibility_offset, 0);
    EXPECT_THAT(file.leaves[1].maxs, ElementsAre(100, 48, 16));
    EXPECT_EQ(file.leaves[1].first_leaf_face, 0);
    EXPECT_EQ(file.leaves[1].leaf_face_count, 1);
    EXPECT_THAT(file.leaves[1].ambient_levels, ElementsAre(10, 20, 30, 40));

    EXPECT_THAT(file.leaf_faces, ElementsAre(0));

    ASSERT_EQ(file.edges.size(), 5u);
    EXPECT_THAT(file.edges[3].vertices, ElementsAre(3, 2));
    EXPECT_THAT(file.face_edges, ElementsAre(1, 2, -3, 4));

    ASSERT_EQ(file.models.size(), 1u);
    EXPECT_EQ(file.models[0].mins.z, -16.0f);
    EXPECT_EQ(file.models[0].maxs.x, 100.0f);
    EXPECT_THAT(file.models[0].head_nodes, ElementsAre(0, 0, 0, 0));
    EXPECT_EQ(file.models[0].leaf_count, 1);
    EXPECT_EQ(file.models[0].first_face, 0);
    EXPECT_EQ(file.models[0].face_count, 1);
  }

  TEST(BspFileTest, ReadsTheTexturesAndLeavesOneThatIsNotCarriedEmpty)
  {
    BspFile file;
    std::string error;
    ASSERT_TRUE(file.Read(MakeQuad().ToBytes(), error)) << error;

    ASSERT_EQ(file.textures.size(), 2u);
    ASSERT_TRUE(file.textures[0].has_value());
    EXPECT_EQ(file.textures[0]->name, "floor");
    EXPECT_EQ(file.textures[0]->width, 16u);
    EXPECT_EQ(file.textures[0]->height, 16u);
    ASSERT_EQ(file.textures[0]->pixels[0].size(), 256u);
    EXPECT_EQ(file.textures[0]->pixels[0][0], 7);
    EXPECT_EQ(file.textures[0]->pixels[3].size(), 4u);

    EXPECT_FALSE(file.textures[1].has_value());
  }

  TEST(BspFileTest, ReadsALevelWithoutTexturesOrLighting)
  {
    Level level = MakeQuad();
    level.Lump(BspLumpKind::Textures).clear();
    level.Lump(BspLumpKind::TextureInfos).clear();
    level.Lump(BspLumpKind::Faces).clear();
    level.Lump(BspLumpKind::LeafFaces).clear();
    level.Lump(BspLumpKind::Lighting).clear();
    // nothing has a face any more
    SetI16(level.Lump(BspLumpKind::Nodes), 22, 0);
    SetI16(level.Lump(BspLumpKind::Leaves), 28 + 22, 0);
    SetI32(level.Lump(BspLumpKind::Models), 60, 0);

    BspFile file;
    std::string error;
    ASSERT_TRUE(file.Read(level.ToBytes(), error)) << error;
    EXPECT_TRUE(file.textures.empty());
    EXPECT_TRUE(file.faces.empty());
    EXPECT_TRUE(file.lighting.empty());
  }

  TEST(BspFileTest, RefusesAnotherVersionAndStaysAsItWas)
  {
    BspFile file;
    std::string error;
    ASSERT_TRUE(file.Read(MakeQuad().ToBytes(), error)) << error;

    Level other = MakeQuad();
    other.version = 30;
    other.Lump(BspLumpKind::Vertices).clear();
    EXPECT_FALSE(file.Read(other.ToBytes(), error));
    EXPECT_THAT(error, HasSubstr("version 30"));

    EXPECT_THAT(error, HasSubstr("BSP2"));

    EXPECT_EQ(file.vertices.size(), 4u);
    EXPECT_EQ(file.format, BspFormat::Version29);
  }

  /// The two forms that lift the limits of version 29.
  constexpr std::array<BspFormat, 2> wide_formats = {BspFormat::Bsp2, BspFormat::Bsp2Rmq};

  /// Where the numbers of the entries of the wider forms lie, in bytes
  /// from the start of an entry. A node and a leaf of `2PSB` have a shorter
  /// box, and what follows it lies earlier.
  constexpr std::size_t wide_node_size = 44;
  constexpr std::size_t rmq_node_size = 32;
  constexpr std::size_t wide_node_children = 4;
  constexpr std::size_t wide_node_maxs = 24;
  constexpr std::size_t wide_node_first_face = 36;
  constexpr std::size_t rmq_node_first_face = 24;
  constexpr std::size_t wide_face_size = 28;
  constexpr std::size_t wide_face_plane = 0;
  constexpr std::size_t wide_face_side = 4;
  constexpr std::size_t wide_face_first_edge = 8;
  constexpr std::size_t wide_face_edge_count = 12;
  constexpr std::size_t wide_face_texture_info = 16;
  constexpr std::size_t wide_face_light_offset = 24;
  constexpr std::size_t wide_clip_node_size = 12;
  constexpr std::size_t wide_clip_node_children = 4;
  constexpr std::size_t wide_leaf_size = 44;
  constexpr std::size_t wide_leaf_maxs = 20;
  constexpr std::size_t wide_leaf_first_face = 32;
  constexpr std::size_t rmq_leaf_size = 32;
  constexpr std::size_t rmq_leaf_first_face = 20;
  constexpr std::size_t wide_edge_size = 8;

  TEST(BspFileTest, ReadsEveryLumpOfTheWiderFormsIntoTheSameEntries)
  {
    BspFile narrow;
    std::string error;
    ASSERT_TRUE(narrow.Read(MakeQuad().ToBytes(), error)) << error;

    for (const BspFormat format : wide_formats)
    {
      const Level level = MakeQuad(format);
      const bool with_fractions = format == BspFormat::Bsp2;
      EXPECT_EQ(level.lumps[static_cast<std::size_t>(BspLumpKind::Nodes)].size(), with_fractions ? 44u : 32u);
      EXPECT_EQ(level.lumps[static_cast<std::size_t>(BspLumpKind::Faces)].size(), 28u);
      EXPECT_EQ(level.lumps[static_cast<std::size_t>(BspLumpKind::ClipNodes)].size(), 12u);
      EXPECT_EQ(level.lumps[static_cast<std::size_t>(BspLumpKind::Leaves)].size(), with_fractions ? 88u : 64u);
      EXPECT_EQ(level.lumps[static_cast<std::size_t>(BspLumpKind::LeafFaces)].size(), 4u);
      EXPECT_EQ(level.lumps[static_cast<std::size_t>(BspLumpKind::Edges)].size(), 40u);

      BspFile file;
      ASSERT_TRUE(file.Read(level.ToBytes(), error)) << error;
      EXPECT_EQ(file.format, format);

      EXPECT_EQ(file.entities, narrow.entities);
      EXPECT_EQ(file.planes.size(), 1u);
      EXPECT_EQ(file.vertices.size(), 4u);
      EXPECT_EQ(file.textures.size(), 2u);
      EXPECT_EQ(file.lighting, narrow.lighting);
      EXPECT_EQ(file.visibility, narrow.visibility);

      ASSERT_EQ(file.nodes.size(), 1u);
      EXPECT_EQ(file.nodes[0].plane, 0);
      EXPECT_THAT(file.nodes[0].children, ElementsAre(-2, -1));
      EXPECT_THAT(file.nodes[0].mins, ElementsAre(0, 0, -16));
      EXPECT_THAT(file.nodes[0].maxs, ElementsAre(100, 48, 16));
      EXPECT_EQ(file.nodes[0].first_face, 0u);
      EXPECT_EQ(file.nodes[0].face_count, 1u);

      ASSERT_EQ(file.faces.size(), 1u);
      EXPECT_EQ(file.faces[0].plane, 0);
      EXPECT_EQ(file.faces[0].side, 0);
      EXPECT_EQ(file.faces[0].first_edge, 0);
      EXPECT_EQ(file.faces[0].edge_count, 4);
      EXPECT_EQ(file.faces[0].texture_info, 0);
      EXPECT_THAT(file.faces[0].styles, ElementsAre(0, 255, 255, 255));
      EXPECT_EQ(file.faces[0].light_offset, 4);

      ASSERT_EQ(file.clip_nodes.size(), 1u);
      EXPECT_EQ(file.clip_nodes[0].plane, 0);
      EXPECT_THAT(file.clip_nodes[0].children, ElementsAre(-1, -2));

      ASSERT_EQ(file.leaves.size(), 2u);
      EXPECT_EQ(file.leaves[0].contents, -2);
      EXPECT_EQ(file.leaves[1].contents, -1);
      EXPECT_EQ(file.leaves[1].visibility_offset, 0);
      EXPECT_THAT(file.leaves[1].mins, ElementsAre(0, 0, 0));
      EXPECT_THAT(file.leaves[1].maxs, ElementsAre(100, 48, 16));
      EXPECT_EQ(file.leaves[1].first_leaf_face, 0u);
      EXPECT_EQ(file.leaves[1].leaf_face_count, 1u);
      EXPECT_THAT(file.leaves[1].ambient_levels, ElementsAre(10, 20, 30, 40));

      EXPECT_THAT(file.leaf_faces, ElementsAre(0));

      ASSERT_EQ(file.edges.size(), 5u);
      EXPECT_THAT(file.edges[1].vertices, ElementsAre(0, 1));
      EXPECT_THAT(file.edges[3].vertices, ElementsAre(3, 2));
      EXPECT_THAT(file.face_edges, ElementsAre(1, 2, -3, 4));

      ASSERT_EQ(file.models.size(), 1u);
      EXPECT_EQ(file.models[0].face_count, 1);
    }
  }

  TEST(BspFileTest, KeepsTheFractionsOfTheBoxesAndTheSideOfAFaceOfBsp2)
  {
    Level level = MakeQuad(BspFormat::Bsp2);
    SetF32(level.Lump(BspLumpKind::Nodes), wide_node_maxs, 100.5f);
    SetF32(level.Lump(BspLumpKind::Leaves), wide_leaf_size + wide_leaf_maxs + 8, 40000.25f);
    SetI32(level.Lump(BspLumpKind::Faces), wide_face_side, 1);

    BspFile file;
    std::string error;
    ASSERT_TRUE(file.Read(level.ToBytes(), error)) << error;
    EXPECT_THAT(file.nodes[0].maxs, ElementsAre(100.5f, 48.0f, 16.0f));
    EXPECT_THAT(file.leaves[1].maxs, ElementsAre(100.0f, 48.0f, 40000.25f));
    EXPECT_EQ(file.faces[0].side, 1);
  }

  /// Makes a lump as long as `count` entries of `entry_size` bytes, the new
  /// ones all zeros.
  void Lengthen(Bytes &lump, const std::size_t entry_size, const std::size_t count)
  {
    lump.resize(entry_size * count, 0);
  }

  TEST(BspFileTest, ReadsNumbersOfTheWiderFormsThatSixteenBitsDoNotHold)
  {
    // 70000 of everything that is named by a number that was widened. The
    // entries added are all zeros, which name the first entry of each lump
    constexpr std::size_t many = 70000;
    constexpr std::int32_t last = 69999;

    for (const BspFormat format : wide_formats)
    {
      const bool with_fractions = format == BspFormat::Bsp2;
      const std::size_t leaf_size = with_fractions ? wide_leaf_size : rmq_leaf_size;
      const std::size_t node_size = with_fractions ? wide_node_size : rmq_node_size;
      const std::size_t node_first_face = with_fractions ? wide_node_first_face : rmq_node_first_face;
      const std::size_t leaf_first_face = with_fractions ? wide_leaf_first_face : rmq_leaf_first_face;

      Level level = MakeQuad(format);
      Lengthen(level.Lump(BspLumpKind::Planes), 20, many);
      Lengthen(level.Lump(BspLumpKind::Vertices), 12, many);
      Lengthen(level.Lump(BspLumpKind::TextureInfos), 40, many);
      Lengthen(level.Lump(BspLumpKind::Nodes), node_size, many);
      Lengthen(level.Lump(BspLumpKind::ClipNodes), wide_clip_node_size, many);
      Lengthen(level.Lump(BspLumpKind::Leaves), leaf_size, many);
      Lengthen(level.Lump(BspLumpKind::LeafFaces), 4, many);
      Lengthen(level.Lump(BspLumpKind::Faces), wide_face_size, many);
      Lengthen(level.Lump(BspLumpKind::Edges), wide_edge_size, many);
      Lengthen(level.Lump(BspLumpKind::FaceEdges), 4, many);

      // a face of zeros has its lightmap at byte 0, which is there
      Bytes &nodes = level.Lump(BspLumpKind::Nodes);
      SetI32(nodes, wide_node_children, last);
      SetI32(nodes, wide_node_children + 4, -static_cast<std::int32_t>(many));
      SetI32(nodes, node_first_face, last);
      SetI32(nodes, node_first_face + 4, 1);

      Bytes &faces = level.Lump(BspLumpKind::Faces);
      SetI32(faces, wide_face_plane, last);
      SetI32(faces, wide_face_first_edge, 0);
      SetI32(faces, wide_face_edge_count, static_cast<std::int32_t>(many));
      SetI32(faces, wide_face_texture_info, last);

      SetI32(level.Lump(BspLumpKind::ClipNodes), wide_clip_node_children, last);
      SetI32(level.Lump(BspLumpKind::ClipNodes), wide_clip_node_children + 4, -5);

      Bytes &leaves = level.Lump(BspLumpKind::Leaves);
      SetI32(leaves, leaf_size + leaf_first_face, last);
      SetI32(leaves, leaf_size + leaf_first_face + 4, 1);

      SetI32(level.Lump(BspLumpKind::LeafFaces), 4 * static_cast<std::size_t>(last), last);
      SetI32(level.Lump(BspLumpKind::Edges), wide_edge_size + 4, last);
      SetI32(level.Lump(BspLumpKind::FaceEdges), 4, -last);

      BspFile file;
      std::string error;
      ASSERT_TRUE(file.Read(level.ToBytes(), error)) << error;

      ASSERT_EQ(file.nodes.size(), many);
      EXPECT_THAT(file.nodes[0].children, ElementsAre(69999, -70000));
      EXPECT_EQ(file.nodes[0].first_face, 69999u);
      EXPECT_EQ(file.nodes[0].face_count, 1u);
      EXPECT_EQ(file.faces[0].plane, 69999);
      EXPECT_EQ(file.faces[0].edge_count, 70000);
      EXPECT_EQ(file.faces[0].texture_info, 69999);
      EXPECT_THAT(file.clip_nodes[0].children, ElementsAre(69999, -5));
      EXPECT_EQ(file.leaves[1].first_leaf_face, 69999u);
      EXPECT_EQ(file.leaf_faces[69999], 69999u);
      EXPECT_THAT(file.edges[1].vertices, ElementsAre(0u, 69999u));
      EXPECT_EQ(file.face_edges[1], -69999);
    }
  }

  TEST(BspFileTest, ReadsTheChildrenOfALargeLevelOfVersion29WithoutASign)
  {
    // 40001 nodes and clip nodes: number 40000 is -25536 where its parent
    // names it, as the compilers of today write it
    constexpr std::size_t many = 40001;
    Level level = MakeQuad();
    Lengthen(level.Lump(BspLumpKind::Nodes), 24, many);
    Lengthen(level.Lump(BspLumpKind::ClipNodes), 8, many);
    SetI16(level.Lump(BspLumpKind::Nodes), 4, static_cast<std::int16_t>(40000));
    SetI16(level.Lump(BspLumpKind::ClipNodes), 4, static_cast<std::int16_t>(40000));

    BspFile file;
    std::string error;
    ASSERT_TRUE(file.Read(level.ToBytes(), error)) << error;
    EXPECT_THAT(file.nodes[0].children, ElementsAre(40000, -1));
    EXPECT_THAT(file.clip_nodes[0].children, ElementsAre(40000, -2));

    // a number that is no node, read without a sign, is still a leaf, and
    // one that is no clip node is still what fills the space
    SetI16(level.Lump(BspLumpKind::Nodes), 4, -2);
    SetI16(level.Lump(BspLumpKind::ClipNodes), 4, -6);
    ASSERT_TRUE(file.Read(level.ToBytes(), error)) << error;
    EXPECT_THAT(file.nodes[0].children, ElementsAre(-2, -1));
    EXPECT_THAT(file.clip_nodes[0].children, ElementsAre(-6, -2));

    // and a leaf that is not there is refused: 50000 is no node, and as a
    // negative number it is leaf 15535
    SetI16(level.Lump(BspLumpKind::Nodes), 4, static_cast<std::int16_t>(50000));
    EXPECT_EQ(ReasonOfRefusal(level), "node 0 names leaf 15535, and there are 2");

    // in a small level a number below 32768 is never a leaf
    Level small = MakeQuad();
    SetI16(small.Lump(BspLumpKind::Nodes), 4, 32767);
    EXPECT_EQ(ReasonOfRefusal(small), "node 0 names node 32767, and there are 1");
    Level small_clip = MakeQuad();
    SetI16(small_clip.Lump(BspLumpKind::ClipNodes), 4, 32767);
    EXPECT_EQ(ReasonOfRefusal(small_clip), "clip node 0 names clip node 32767, and there are 1");
  }

  TEST(BspFileTest, RefusesALumpOfAWiderFormThatDoesNotHoldWholeEntries)
  {
    // the lumps of version 29 under the letters of another form
    Level narrow = MakeQuad();
    narrow.version = BspFile::bsp2_magic;
    EXPECT_EQ(ReasonOfRefusal(narrow),
      "the lump of nodes has 24 bytes, which is not a multiple of the 44 of an entry");
    narrow.version = BspFile::bsp2_rmq_magic;
    EXPECT_EQ(ReasonOfRefusal(narrow),
      "the lump of nodes has 24 bytes, which is not a multiple of the 32 of an entry");

    Level faces = MakeQuad(BspFormat::Bsp2);
    faces.Lump(BspLumpKind::Faces).resize(20);
    EXPECT_EQ(ReasonOfRefusal(faces), "the lump of faces has 20 bytes, which is not a multiple of the 28 of an entry");

    Level clip_nodes = MakeQuad(BspFormat::Bsp2);
    clip_nodes.Lump(BspLumpKind::ClipNodes).resize(8);
    EXPECT_EQ(ReasonOfRefusal(clip_nodes),
      "the lump of clip nodes has 8 bytes, which is not a multiple of the 12 of an entry");

    // the leaves of one wider form are not those of the other
    Level leaves = MakeQuad(BspFormat::Bsp2Rmq);
    leaves.version = BspFile::bsp2_magic;
    leaves.Lump(BspLumpKind::Nodes) = MakeQuad(BspFormat::Bsp2).Lump(BspLumpKind::Nodes);
    EXPECT_EQ(ReasonOfRefusal(leaves),
      "the lump of leaves has 64 bytes, which is not a multiple of the 44 of an entry");

    Level leaf_faces = MakeQuad(BspFormat::Bsp2);
    leaf_faces.Lump(BspLumpKind::LeafFaces).resize(2);
    EXPECT_EQ(ReasonOfRefusal(leaf_faces),
      "the lump of faces of leaves has 2 bytes, which is not a multiple of the 4 of an entry");

    Level edges = MakeQuad(BspFormat::Bsp2);
    edges.Lump(BspLumpKind::Edges).resize(36);
    EXPECT_EQ(ReasonOfRefusal(edges), "the lump of edges has 36 bytes, which is not a multiple of the 8 of an entry");
  }

  TEST(BspFileTest, RefusesAnEntryOfAWiderFormThatNamesWhatIsNotThere)
  {
    for (const BspFormat format : wide_formats)
    {
      const bool with_fractions = format == BspFormat::Bsp2;
      const std::size_t leaf_size = with_fractions ? wide_leaf_size : rmq_leaf_size;
      const std::size_t node_first_face = with_fractions ? wide_node_first_face : rmq_node_first_face;
      const std::size_t leaf_first_face = with_fractions ? wide_leaf_first_face : rmq_leaf_first_face;

      // numbers that 16 bits do not hold, and negative ones, which version
      // 29 has no room for
      Level plane = MakeQuad(format);
      SetI32(plane.Lump(BspLumpKind::Faces), wide_face_plane, 65536);
      EXPECT_EQ(ReasonOfRefusal(plane), "face 0 names plane 65536, and there are 1");
      SetI32(plane.Lump(BspLumpKind::Faces), wide_face_plane, -1);
      EXPECT_EQ(ReasonOfRefusal(plane), "face 0 names plane -1, and there are 1");

      Level info = MakeQuad(format);
      SetI32(info.Lump(BspLumpKind::Faces), wide_face_texture_info, 65536);
      EXPECT_EQ(ReasonOfRefusal(info), "face 0 names texture info 65536, and there are 1");
      SetI32(info.Lump(BspLumpKind::Faces), wide_face_texture_info, -1);
      EXPECT_EQ(ReasonOfRefusal(info), "face 0 names texture info -1, and there are 1");

      Level edge_count = MakeQuad(format);
      SetI32(edge_count.Lump(BspLumpKind::Faces), wide_face_edge_count, 65540);
      EXPECT_EQ(ReasonOfRefusal(edge_count), "face 0 names 65540 edges of faces from 0, and there are 4");
      SetI32(edge_count.Lump(BspLumpKind::Faces), wide_face_edge_count, -4);
      EXPECT_EQ(ReasonOfRefusal(edge_count), "face 0 names -4 edges of faces from 0, and there are 4");

      Level first_edge = MakeQuad(format);
      SetI32(first_edge.Lump(BspLumpKind::Faces), wide_face_first_edge, 2147483647);
      EXPECT_EQ(ReasonOfRefusal(first_edge), "face 0 names 4 edges of faces from 2147483647, and there are 4");

      Level light = MakeQuad(format);
      SetI32(light.Lump(BspLumpKind::Faces), wide_face_light_offset, 36);
      EXPECT_EQ(ReasonOfRefusal(light), "face 0 names byte of lighting 36, and there are 36");

      Level node = MakeQuad(format);
      SetI32(node.Lump(BspLumpKind::Nodes), wide_node_children, 65536);
      EXPECT_EQ(ReasonOfRefusal(node), "node 0 names node 65536, and there are 1");

      // what version 29 would read as node 0 or leaf 0 is neither here
      Level leaf = MakeQuad(format);
      SetI32(leaf.Lump(BspLumpKind::Nodes), wide_node_children + 4, -65537);
      EXPECT_EQ(ReasonOfRefusal(leaf), "node 0 names leaf 65536, and there are 2");
      SetI32(leaf.Lump(BspLumpKind::Nodes), wide_node_children + 4, -2147483647 - 1);
      EXPECT_EQ(ReasonOfRefusal(leaf), "node 0 names leaf 2147483647, and there are 2");

      Level node_faces = MakeQuad(format);
      SetI32(node_faces.Lump(BspLumpKind::Nodes), node_first_face, -1);
      EXPECT_EQ(ReasonOfRefusal(node_faces), "node 0 names 1 faces from 4294967295, and there are 1");
      SetI32(node_faces.Lump(BspLumpKind::Nodes), node_first_face, 0);
      SetI32(node_faces.Lump(BspLumpKind::Nodes), node_first_face + 4, 65537);
      EXPECT_EQ(ReasonOfRefusal(node_faces), "node 0 names 65537 faces from 0, and there are 1");

      Level clip_node = MakeQuad(format);
      SetI32(clip_node.Lump(BspLumpKind::ClipNodes), wide_clip_node_children, 65536);
      EXPECT_EQ(ReasonOfRefusal(clip_node), "clip node 0 names clip node 65536, and there are 1");

      Level leaf_faces = MakeQuad(format);
      SetI32(leaf_faces.Lump(BspLumpKind::Leaves), leaf_size + leaf_first_face, 65536);
      EXPECT_EQ(ReasonOfRefusal(leaf_faces), "leaf 1 names 1 faces of leaves from 65536, and there are 1");
      SetI32(leaf_faces.Lump(BspLumpKind::Leaves), leaf_size + leaf_first_face, 0);
      SetI32(leaf_faces.Lump(BspLumpKind::Leaves), leaf_size + leaf_first_face + 4, -1);
      EXPECT_EQ(ReasonOfRefusal(leaf_faces), "leaf 1 names 4294967295 faces of leaves from 0, and there are 1");

      Level leaf_face = MakeQuad(format);
      SetI32(leaf_face.Lump(BspLumpKind::LeafFaces), 0, 65536);
      EXPECT_EQ(ReasonOfRefusal(leaf_face), "face of leaves 0 names face 65536, and there are 1");

      Level vertex = MakeQuad(format);
      SetI32(vertex.Lump(BspLumpKind::Edges), 2 * wide_edge_size + 4, 65538);
      EXPECT_EQ(ReasonOfRefusal(vertex), "edge 2 names vertex 65538, and there are 4");
    }
  }

  TEST(BspFileTest, NeverReadsOutsideAFileOfAWiderFormCutAnywhere)
  {
    for (const BspFormat format : wide_formats)
    {
      const Bytes whole = MakeQuad(format).ToBytes();
      for (std::size_t size = 0; size < whole.size(); size++)
      {
        const Bytes cut(whole.begin(), whole.begin() + static_cast<std::ptrdiff_t>(size));
        BspFile file;
        std::string error;
        EXPECT_FALSE(file.Read(cut, error)) << size;
        EXPECT_FALSE(error.empty()) << size;
      }
    }
  }

  TEST(BspFileTest, RefusesAFileShorterThanItsHeader)
  {
    BspFile file;
    std::string error;

    EXPECT_FALSE(file.Read({}, error));
    EXPECT_THAT(error, HasSubstr("0 bytes"));

    const Bytes three = {29, 0, 0};
    EXPECT_FALSE(file.Read(three, error));
    EXPECT_THAT(error, HasSubstr("3 bytes"));

    Bytes cut = MakeQuad().ToBytes();
    cut.resize(123);
    EXPECT_FALSE(file.Read(cut, error));
    EXPECT_THAT(error, HasSubstr("123 bytes"));
    EXPECT_THAT(error, HasSubstr("header"));
  }

  TEST(BspFileTest, RefusesALumpThatLiesOutsideTheFile)
  {
    BspFile file;
    std::string error;

    // the file is cut short, so that the last lump ends past its end
    Bytes cut = MakeQuad().ToBytes();
    cut.pop_back();
    EXPECT_FALSE(file.Read(cut, error));
    EXPECT_THAT(error, HasSubstr("the lump of models"));
    EXPECT_THAT(error, HasSubstr("outside the file"));

    // the place of the planes, the second lump, is past the end
    Bytes far = MakeQuad().ToBytes();
    SetI32(far, 4 + 8, static_cast<std::int32_t>(far.size()) + 1);
    EXPECT_FALSE(file.Read(far, error));
    EXPECT_THAT(error, HasSubstr("the lump of planes"));

    Bytes negative_place = MakeQuad().ToBytes();
    SetI32(negative_place, 4 + 8, -20);
    EXPECT_FALSE(file.Read(negative_place, error));
    EXPECT_THAT(error, HasSubstr("the lump of planes"));

    Bytes negative_size = MakeQuad().ToBytes();
    SetI32(negative_size, 4 + 8 + 4, -20);
    EXPECT_FALSE(file.Read(negative_size, error));
    EXPECT_THAT(error, HasSubstr("the lump of planes"));

    // a size so large that adding it to the place would wrap around
    Bytes wrapping = MakeQuad().ToBytes();
    SetI32(wrapping, 4 + 8 + 4, 0x7fffffff);
    EXPECT_FALSE(file.Read(wrapping, error));
    EXPECT_THAT(error, HasSubstr("the lump of planes"));
  }

  TEST(BspFileTest, RefusesALumpThatDoesNotHoldWholeEntries)
  {
    const std::array<std::pair<BspLumpKind, const char *>, 11> lumps = {{
      {BspLumpKind::Planes, "planes"},
      {BspLumpKind::Vertices, "vertices"},
      {BspLumpKind::Nodes, "nodes"},
      {BspLumpKind::TextureInfos, "texture infos"},
      {BspLumpKind::Faces, "faces"},
      {BspLumpKind::ClipNodes, "clip nodes"},
      {BspLumpKind::Leaves, "leaves"},
      {BspLumpKind::LeafFaces, "faces of leaves"},
      {BspLumpKind::Edges, "edges"},
      {BspLumpKind::FaceEdges, "edges of faces"},
      {BspLumpKind::Models, "models"},
    }};

    for (const auto &[kind, name] : lumps)
    {
      Level level = MakeQuad();
      level.Lump(kind).push_back(0);

      const std::string error = ReasonOfRefusal(level);
      EXPECT_THAT(error, HasSubstr(std::string("the lump of ") + name + " has")) << name;
      EXPECT_THAT(error, HasSubstr("not a multiple")) << name;
    }
  }

  TEST(BspFileTest, RefusesALumpOfTexturesThatLiesAboutItself)
  {
    Level short_count = MakeQuad();
    short_count.Lump(BspLumpKind::Textures).resize(3);
    EXPECT_THAT(ReasonOfRefusal(short_count), HasSubstr("fewer than the 4 of its count"));

    Level many = MakeQuad();
    SetI32(many.Lump(BspLumpKind::Textures), 0, 1000000);
    EXPECT_THAT(ReasonOfRefusal(many), HasSubstr("says it has 1000000 textures"));

    Level negative = MakeQuad();
    SetI32(negative.Lump(BspLumpKind::Textures), 0, -5);
    EXPECT_THAT(ReasonOfRefusal(negative), HasSubstr("says it has -5 textures"));

    Level far = MakeQuad();
    SetI32(far.Lump(BspLumpKind::Textures), 4, 100000);
    EXPECT_THAT(ReasonOfRefusal(far), HasSubstr("texture 0 starts at byte 100000"));

    Level before = MakeQuad();
    SetI32(before.Lump(BspLumpKind::Textures), 4, -2);
    EXPECT_THAT(ReasonOfRefusal(before), HasSubstr("texture 0 starts at byte -2"));

    // the pictures of the texture are cut short
    Level cut = MakeQuad();
    cut.Lump(BspLumpKind::Textures).pop_back();
    EXPECT_THAT(ReasonOfRefusal(cut), HasSubstr("texture 0: size 3 of \"floor\""));

    // the texture starts so late that not even its header is there
    Level late = MakeQuad();
    SetI32(late.Lump(BspLumpKind::Textures), 4, static_cast<std::int32_t>(late.Lump(BspLumpKind::Textures).size()));
    EXPECT_THAT(ReasonOfRefusal(late), HasSubstr("texture 0: it has 0 bytes"));
  }

  TEST(BspFileTest, RefusesAFaceThatNamesWhatIsNotThere)
  {
    Level plane = MakeQuad();
    SetI16(plane.Lump(BspLumpKind::Faces), 0, 1);
    EXPECT_EQ(ReasonOfRefusal(plane), "face 0 names plane 1, and there are 1");

    Level info = MakeQuad();
    SetI16(info.Lump(BspLumpKind::Faces), 10, 1);
    EXPECT_EQ(ReasonOfRefusal(info), "face 0 names texture info 1, and there are 1");

    Level edges = MakeQuad();
    SetI16(edges.Lump(BspLumpKind::Faces), 8, 5);
    EXPECT_EQ(ReasonOfRefusal(edges), "face 0 names 5 edges of faces from 0, and there are 4");

    Level first_edge = MakeQuad();
    SetI32(first_edge.Lump(BspLumpKind::Faces), 4, -1);
    EXPECT_EQ(ReasonOfRefusal(first_edge), "face 0 names 4 edges of faces from -1, and there are 4");

    Level light = MakeQuad();
    SetI32(light.Lump(BspLumpKind::Faces), 16, 36);
    EXPECT_EQ(ReasonOfRefusal(light), "face 0 names byte of lighting 36, and there are 36");

    Level dark = MakeQuad();
    SetI32(dark.Lump(BspLumpKind::Faces), 16, -2);
    EXPECT_EQ(ReasonOfRefusal(dark), "face 0 names byte of lighting -2, and there are 36");

    // a face without a lightmap is fine
    Level unlit = MakeQuad();
    SetI32(unlit.Lump(BspLumpKind::Faces), 16, -1);
    EXPECT_EQ(ReasonOfRefusal(unlit), "");
  }

  TEST(BspFileTest, RefusesAnEdgeOrATextureInfoThatNamesWhatIsNotThere)
  {
    Level face_edge = MakeQuad();
    SetI32(face_edge.Lump(BspLumpKind::FaceEdges), 4, 5);
    EXPECT_EQ(ReasonOfRefusal(face_edge), "edge of faces 1 names edge 5, and there are 5");

    Level backwards = MakeQuad();
    SetI32(backwards.Lump(BspLumpKind::FaceEdges), 8, -5);
    EXPECT_EQ(ReasonOfRefusal(backwards), "edge of faces 2 names edge 5, and there are 5");

    // the lowest number there is, which has no positive form
    Level lowest = MakeQuad();
    SetI32(lowest.Lump(BspLumpKind::FaceEdges), 8, -2147483647 - 1);
    EXPECT_EQ(ReasonOfRefusal(lowest), "edge of faces 2 names edge 2147483648, and there are 5");

    Level vertex = MakeQuad();
    SetI16(vertex.Lump(BspLumpKind::Edges), 4 * 2 + 2, 4);
    EXPECT_EQ(ReasonOfRefusal(vertex), "edge 2 names vertex 4, and there are 4");

    Level texture = MakeQuad();
    SetI32(texture.Lump(BspLumpKind::TextureInfos), 32, 2);
    EXPECT_EQ(ReasonOfRefusal(texture), "texture info 0 names texture 2, and there are 2");

    Level no_texture = MakeQuad();
    SetI32(no_texture.Lump(BspLumpKind::TextureInfos), 32, -1);
    EXPECT_EQ(ReasonOfRefusal(no_texture), "texture info 0 names texture -1, and there are 2");
  }

  TEST(BspFileTest, RefusesANodeOrAClipNodeThatNamesWhatIsNotThere)
  {
    Level plane = MakeQuad();
    SetI32(plane.Lump(BspLumpKind::Nodes), 0, 3);
    EXPECT_EQ(ReasonOfRefusal(plane), "node 0 names plane 3, and there are 1");

    Level node = MakeQuad();
    SetI16(node.Lump(BspLumpKind::Nodes), 4, 1);
    EXPECT_EQ(ReasonOfRefusal(node), "node 0 names node 1, and there are 1");

    Level leaf = MakeQuad();
    SetI16(leaf.Lump(BspLumpKind::Nodes), 6, -3);
    EXPECT_EQ(ReasonOfRefusal(leaf), "node 0 names leaf 2, and there are 2");

    Level faces = MakeQuad();
    SetI16(faces.Lump(BspLumpKind::Nodes), 20, 1);
    EXPECT_EQ(ReasonOfRefusal(faces), "node 0 names 1 faces from 1, and there are 1");

    Level clip_plane = MakeQuad();
    SetI32(clip_plane.Lump(BspLumpKind::ClipNodes), 0, -1);
    EXPECT_EQ(ReasonOfRefusal(clip_plane), "clip node 0 names plane -1, and there are 1");

    Level clip_node = MakeQuad();
    SetI16(clip_node.Lump(BspLumpKind::ClipNodes), 6, 1);
    EXPECT_EQ(ReasonOfRefusal(clip_node), "clip node 0 names clip node 1, and there are 1");

    // a negative child of a clip node is what fills the space, whatever it is
    Level sky = MakeQuad();
    SetI16(sky.Lump(BspLumpKind::ClipNodes), 6, -6);
    EXPECT_EQ(ReasonOfRefusal(sky), "");
  }

  TEST(BspFileTest, ReadsALevelWithoutVisibilityWhoseLeavesNameItAllTheSame)
  {
    // as the small levels that are items do
    Level level = MakeQuad();
    level.Lump(BspLumpKind::Visibility).clear();
    SetI32(level.Lump(BspLumpKind::Leaves), 28 + 4, 0);
    EXPECT_EQ(ReasonOfRefusal(level), "");
  }

  TEST(BspFileTest, RefusesALeafThatNamesWhatIsNotThere)
  {
    Level faces = MakeQuad();
    SetI16(faces.Lump(BspLumpKind::Leaves), 28 + 22, 2);
    EXPECT_EQ(ReasonOfRefusal(faces), "leaf 1 names 2 faces of leaves from 0, and there are 1");

    Level visibility = MakeQuad();
    SetI32(visibility.Lump(BspLumpKind::Leaves), 28 + 4, 1);
    EXPECT_EQ(ReasonOfRefusal(visibility), "leaf 1 names byte of visibility 1, and there are 1");

    Level leaf_face = MakeQuad();
    SetI16(leaf_face.Lump(BspLumpKind::LeafFaces), 0, 1);
    EXPECT_EQ(ReasonOfRefusal(leaf_face), "face of leaves 0 names face 1, and there are 1");
  }

  TEST(BspFileTest, RefusesAModelThatNamesWhatIsNotThere)
  {
    Level faces = MakeQuad();
    SetI32(faces.Lump(BspLumpKind::Models), 60, 2);
    EXPECT_EQ(ReasonOfRefusal(faces), "model 0 names 2 faces from 0, and there are 1");

    Level negative = MakeQuad();
    SetI32(negative.Lump(BspLumpKind::Models), 60, -1);
    EXPECT_EQ(ReasonOfRefusal(negative), "model 0 names -1 faces from 0, and there are 1");

    Level node = MakeQuad();
    SetI32(node.Lump(BspLumpKind::Models), 36, 1);
    EXPECT_EQ(ReasonOfRefusal(node), "model 0 names node 1, and there are 1");

    Level leaf = MakeQuad();
    SetI32(leaf.Lump(BspLumpKind::Models), 36, -3);
    EXPECT_EQ(ReasonOfRefusal(leaf), "model 0 names leaf 2, and there are 2");

    // a model that is a single leaf is fine
    Level one_leaf = MakeQuad();
    SetI32(one_leaf.Lump(BspLumpKind::Models), 36, -2);
    EXPECT_EQ(ReasonOfRefusal(one_leaf), "");

    Level clip_node = MakeQuad();
    SetI32(clip_node.Lump(BspLumpKind::Models), 44, 1);
    EXPECT_EQ(ReasonOfRefusal(clip_node), "model 0 names clip node 1, and there are 1");

    // the fourth tree is not used by the game, and is not looked at
    Level fourth = MakeQuad();
    SetI32(fourth.Lump(BspLumpKind::Models), 48, 99);
    EXPECT_EQ(ReasonOfRefusal(fourth), "");
  }

  TEST(BspFileTest, NeverReadsOutsideAFileCutAnywhere)
  {
    const Bytes whole = MakeQuad().ToBytes();

    // every length short of the whole is refused, and none of them is read past its end
    for (std::size_t size = 0; size < whole.size(); size++)
    {
      const Bytes cut(whole.begin(), whole.begin() + static_cast<std::ptrdiff_t>(size));
      BspFile file;
      std::string error;
      EXPECT_FALSE(file.Read(cut, error)) << size;
      EXPECT_FALSE(error.empty()) << size;
    }
  }

  TEST(BspFileTest, ReadsTheLevelsOfRealDataWhenTheyAreThere)
  {
    const std::filesystem::path maps = std::filesystem::path(QUAKE_TEST_DATA_DIRECTORY) / "maps";

    std::vector<std::filesystem::path> levels;
    std::error_code ignored;
    for (std::filesystem::directory_iterator entry(maps, ignored), end; !ignored && entry != end;
      entry.increment(ignored))
    {
      if (entry->path().extension() == ".bsp") { levels.push_back(entry->path()); }
    }
    if (levels.empty()) { GTEST_SKIP() << "No compiled level in " << maps.string(); }

    std::size_t read = 0;
    for (const std::filesystem::path &path : levels)
    {
      std::ifstream stream(path, std::ios::binary);
      const Bytes bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

      BspFile file;
      std::string error;
      EXPECT_TRUE(file.Read(bytes, error)) << path.string() << ": " << error;
      EXPECT_FALSE(file.models.empty()) << path.string();
      read++;
    }
    EXPECT_GT(read, 0u);
  }

  TEST(BspFileTest, ReadsEveryLevelOfThePaksAndTheOneThatIsTooLargeForVersion29)
  {
    const RealData &data = RealData::Get();
    if (!data.IsThere()) { GTEST_SKIP() << data.GetProblem(); }

    std::size_t levels = 0;
    for (const std::string &name : data.ListNames("maps"))
    {
      if (!name.ends_with(".bsp")) { continue; }

      BspFile file;
      std::string error;
      EXPECT_TRUE(file.Read(data.GetBytes(name), error)) << name << ": " << error;
      EXPECT_FALSE(file.models.empty()) << name;
      EXPECT_EQ(file.format, name == "maps/lq_e2m4.bsp" ? BspFormat::Bsp2 : BspFormat::Version29) << name;
      levels++;
    }
    EXPECT_GT(levels, 3u);

    // the level of the wider form has more of something than 16 bits count,
    // which is why its compiler wrote it so
    BspFile large;
    std::string error;
    ASSERT_TRUE(large.Read(data.GetBytes("maps/lq_e2m4.bsp"), error)) << error;
    std::cout << "lq_e2m4: " << large.vertices.size() << " vertices, " << large.edges.size() << " edges, "
              << large.faces.size() << " faces, " << large.leaf_faces.size() << " faces of leaves, "
              << large.nodes.size() << " nodes, " << large.clip_nodes.size() << " clip nodes, "
              << large.leaves.size() << " leaves, " << large.planes.size() << " planes\n";
    EXPECT_TRUE(
      large.vertices.size() > 65535 || large.edges.size() > 65535 || large.faces.size() > 65535 ||
      large.leaf_faces.size() > 65535 || large.nodes.size() > 32767 || large.clip_nodes.size() > 32767 ||
      large.leaves.size() > 32767 || large.planes.size() > 65535);
  }
}
