#include "bsp-file.hpp"

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{
  using quake::BspFile;
  using quake::BspLumpKind;
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
  Level MakeQuad()
  {
    Level level;

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
    PutI16(nodes, -2);
    PutI16(nodes, -1);
    for (const std::int16_t number : {0, 0, -16, 100, 48, 16}) { PutI16(nodes, number); }
    PutU16(nodes, 0);
    PutU16(nodes, 1);

    Bytes &infos = level.Lump(BspLumpKind::TextureInfos);
    PutVector(infos, 1.0f, 0.0f, 0.0f);
    PutF32(infos, 8.0f);
    PutVector(infos, 0.0f, -1.0f, 0.0f);
    PutF32(infos, 0.0f);
    PutI32(infos, 0);
    PutI32(infos, 0);

    Bytes &faces = level.Lump(BspLumpKind::Faces);
    PutU16(faces, 0);
    PutI16(faces, 0);
    PutI32(faces, 0);
    PutU16(faces, 4);
    PutU16(faces, 0);
    for (const std::uint8_t style : {0, 255, 255, 255}) { PutU8(faces, style); }
    PutI32(faces, 4);

    // four bytes of nobody, then the 8 by 4 samples of the face
    Bytes &lighting = level.Lump(BspLumpKind::Lighting);
    lighting.assign(4, 0xaa);
    for (int i = 0; i < 32; i++) { PutU8(lighting, static_cast<std::uint8_t>(i)); }

    Bytes &clip_nodes = level.Lump(BspLumpKind::ClipNodes);
    PutI32(clip_nodes, 0);
    PutI16(clip_nodes, -1);
    PutI16(clip_nodes, -2);

    // leaf 0 is everything solid, leaf 1 is the room above the floor
    Bytes &leaves = level.Lump(BspLumpKind::Leaves);
    PutI32(leaves, -2);
    PutI32(leaves, -1);
    for (int i = 0; i < 6; i++) { PutI16(leaves, 0); }
    PutU16(leaves, 0);
    PutU16(leaves, 0);
    for (int i = 0; i < 4; i++) { PutU8(leaves, 0); }
    PutI32(leaves, -1);
    PutI32(leaves, 0);
    for (const std::int16_t number : {0, 0, 0, 100, 48, 16}) { PutI16(leaves, number); }
    PutU16(leaves, 0);
    PutU16(leaves, 1);
    for (const std::uint8_t level_of_sound : {10, 20, 30, 40}) { PutU8(leaves, level_of_sound); }

    PutU16(level.Lump(BspLumpKind::LeafFaces), 0);

    // edge 0 is never used, since an edge of a face says its direction by its sign
    Bytes &edges = level.Lump(BspLumpKind::Edges);
    for (const std::uint16_t vertex : {0, 0, 0, 1, 1, 2, 3, 2, 3, 0}) { PutU16(edges, vertex); }

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

    // the level of the tools that lift the limits, whose version is the letters "BSP2"
    other.version = 0x32505342;
    EXPECT_FALSE(file.Read(other.ToBytes(), error));
    EXPECT_THAT(error, HasSubstr("version"));

    EXPECT_EQ(file.vertices.size(), 4u);
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

      // a level of another version is not what this reads
      if (bytes.size() < 4 || bytes[0] != 29 || bytes[1] != 0 || bytes[2] != 0 || bytes[3] != 0) { continue; }

      BspFile file;
      std::string error;
      EXPECT_TRUE(file.Read(bytes, error)) << path.string() << ": " << error;
      EXPECT_FALSE(file.models.empty()) << path.string();
      read++;
    }
    if (read == 0) { GTEST_SKIP() << "No level of version 29 in " << maps.string(); }
  }
}
