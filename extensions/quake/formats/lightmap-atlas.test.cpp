#include "lightmap-atlas.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "bsp-file.hpp"

namespace
{
  using quake::BspFile;
  using quake::BspMesh;
  using quake::BspMeshFace;
  using quake::BspMeshVertex;
  using quake::LightmapAtlas;
  using quake::LightmapAtlasBlock;
  using quake::LightmapAtlasPage;
  using quake::LightmapAtlasVertex;
  using ::testing::Each;
  using ::testing::ElementsAre;
  using ::testing::IsEmpty;

  /// Adds a face to a mesh with a lightmap of a size and these samples,
  /// lit by the one light that never changes. Without samples it has no
  /// lightmap. Its corners lie where `corners` says on its own lightmap,
  /// or, when none are given, on the middles of its four outermost samples.
  BspMeshFace &AddFace(
    BspMesh &mesh,
    const std::uint32_t width,
    const std::uint32_t height,
    const std::vector<std::uint8_t> &samples,
    std::vector<std::pair<float, float>> corners = {})
  {
    if (corners.empty() && width > 0 && height > 0)
    {
      const float left = 0.5f / static_cast<float>(width);
      const float top = 0.5f / static_cast<float>(height);
      corners = {{left, top}, {1.0f - left, top}, {1.0f - left, 1.0f - top}, {left, 1.0f - top}};
    }
    if (corners.empty()) { corners.assign(4, {0.0f, 0.0f}); }

    BspMeshFace face;
    face.face = static_cast<std::uint32_t>(mesh.faces.size());
    face.first_vertex = static_cast<std::uint32_t>(mesh.vertices.size());
    face.vertex_count = static_cast<std::uint32_t>(corners.size());
    face.lightmap_width = width;
    face.lightmap_height = height;
    face.light_styles = {0, 255, 255, 255};
    face.light_offset = samples.empty() ? -1 : 0;
    face.lightmap = samples;

    for (const auto &[u, v] : corners)
    {
      BspMeshVertex vertex;
      vertex.lightmap_u = u;
      vertex.lightmap_v = v;
      mesh.vertices.push_back(vertex);
    }

    mesh.faces.push_back(std::move(face));
    return mesh.faces.back();
  }

  /// Samples for a lightmap of a size that count up from a number, so that
  /// every sample of a face, and of the faces next to it, is another.
  std::vector<std::uint8_t> CountUp(const std::uint32_t width, const std::uint32_t height, const int from)
  {
    std::vector<std::uint8_t> samples;
    for (std::uint32_t i = 0; i < width * height; i++) { samples.push_back(static_cast<std::uint8_t>(from + i)); }
    return samples;
  }

  /// The light of a pixel of a page. It also checks that the pixel is grey
  /// and solid.
  std::uint8_t PixelAt(const LightmapAtlasPage &page, const std::uint32_t x, const std::uint32_t y)
  {
    EXPECT_LT(x, page.width);
    EXPECT_LT(y, page.height);
    const std::size_t at = (static_cast<std::size_t>(y) * page.width + x) * 4;
    EXPECT_EQ(page.pixels[at + 1], page.pixels[at]);
    EXPECT_EQ(page.pixels[at + 2], page.pixels[at]);
    EXPECT_EQ(page.pixels[at + 3], 255);
    return page.pixels[at];
  }

  /// The light of the pixel the coordinates of a vertex point into, which
  /// is what a renderer that does not blend pixels would draw there.
  std::uint8_t SampleAt(const LightmapAtlas &atlas, const LightmapAtlasVertex &vertex)
  {
    EXPECT_LT(vertex.page, atlas.pages.size());
    const LightmapAtlasPage &page = atlas.pages[vertex.page];
    const auto x = static_cast<std::uint32_t>(vertex.u * static_cast<float>(page.width));
    const auto y = static_cast<std::uint32_t>(vertex.v * static_cast<float>(page.height));
    return PixelAt(page, x, y);
  }

  /// What packing a mesh says is wrong. Empty when nothing is.
  std::string ReasonOfRefusal(
    const BspMesh &mesh,
    const std::uint32_t largest_side = LightmapAtlas::default_largest_side)
  {
    LightmapAtlas atlas;
    std::string error;
    if (atlas.Build(mesh, error, largest_side)) { return {}; }
    EXPECT_FALSE(error.empty());
    return error;
  }

  TEST(LightmapAtlasTest, PutsTheLightmapOfOneFaceOnOnePageAsItsBytesAre)
  {
    BspMesh mesh;
    AddFace(mesh, 2, 3, {0, 64, 128, 200, 254, 255});

    LightmapAtlas atlas;
    std::string error;
    ASSERT_TRUE(atlas.Build(mesh, error)) << error;

    // 2 by 3 samples and a border are 4 by 5 pixels, which 8 by 8 holds
    EXPECT_EQ(atlas.side, 8u);
    ASSERT_EQ(atlas.pages.size(), 1u);
    EXPECT_EQ(atlas.pages[0].width, 8u);
    EXPECT_EQ(atlas.pages[0].height, 8u);
    EXPECT_EQ(atlas.pages[0].pixels.size(), 8u * 8u * 4u);

    ASSERT_EQ(atlas.blocks.size(), 1u);
    const LightmapAtlasBlock &block = atlas.blocks[0];
    EXPECT_TRUE(block.is_lit);
    EXPECT_EQ(block.page, 0u);
    EXPECT_EQ(block.x, 1u);
    EXPECT_EQ(block.y, 1u);
    EXPECT_EQ(block.width, 2u);
    EXPECT_EQ(block.height, 3u);

    // nothing is doubled: 128 stays 128, and 255 stays 255
    EXPECT_EQ(PixelAt(atlas.pages[0], 1, 1), 0);
    EXPECT_EQ(PixelAt(atlas.pages[0], 2, 1), 64);
    EXPECT_EQ(PixelAt(atlas.pages[0], 1, 2), 128);
    EXPECT_EQ(PixelAt(atlas.pages[0], 2, 2), 200);
    EXPECT_EQ(PixelAt(atlas.pages[0], 1, 3), 254);
    EXPECT_EQ(PixelAt(atlas.pages[0], 2, 3), 255);

    // a pixel no face uses is black and solid
    EXPECT_EQ(PixelAt(atlas.pages[0], 7, 7), 0);

    ASSERT_EQ(atlas.vertices.size(), 4u);
    EXPECT_EQ(SampleAt(atlas, atlas.vertices[0]), 0);
    EXPECT_EQ(SampleAt(atlas, atlas.vertices[1]), 64);
    EXPECT_EQ(SampleAt(atlas, atlas.vertices[2]), 255);
    EXPECT_EQ(SampleAt(atlas, atlas.vertices[3]), 254);
  }

  TEST(LightmapAtlasTest, RepeatsTheOutermostSamplesOfAFaceInABorderAroundIt)
  {
    BspMesh mesh;
    AddFace(mesh, 3, 2, {10, 20, 30, 40, 50, 60});

    LightmapAtlas atlas;
    std::string error;
    ASSERT_TRUE(atlas.Build(mesh, error)) << error;
    ASSERT_EQ(atlas.pages.size(), 1u);
    ASSERT_EQ(atlas.blocks[0].x, 1u);
    ASSERT_EQ(atlas.blocks[0].y, 1u);

    const std::vector<std::vector<std::uint8_t>> expected = {
      {10, 10, 20, 30, 30},
      {10, 10, 20, 30, 30},
      {40, 40, 50, 60, 60},
      {40, 40, 50, 60, 60}};
    for (std::uint32_t y = 0; y < 4; y++)
    {
      for (std::uint32_t x = 0; x < 5; x++)
      {
        EXPECT_EQ(PixelAt(atlas.pages[0], x, y), expected[y][x]) << "pixel " << x << ", " << y;
      }
    }

    // and nothing beyond the border
    EXPECT_EQ(PixelAt(atlas.pages[0], 5, 0), 0);
    EXPECT_EQ(PixelAt(atlas.pages[0], 0, 4), 0);
  }

  TEST(LightmapAtlasTest, PutsFacesOfDifferentSizesNextToEachOtherWithoutOneTouchingAnother)
  {
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> sizes = {{3, 2}, {5, 7}, {1, 1}, {4, 4}, {9, 2}, {2, 6}};

    BspMesh mesh;
    for (std::size_t i = 0; i < sizes.size(); i++)
    {
      const auto [width, height] = sizes[i];
      AddFace(mesh, width, height, CountUp(width, height, static_cast<int>(i) * 40));
    }

    LightmapAtlas atlas;
    std::string error;
    ASSERT_TRUE(atlas.Build(mesh, error)) << error;

    ASSERT_EQ(atlas.pages.size(), 1u);
    ASSERT_EQ(atlas.blocks.size(), sizes.size());
    ASSERT_EQ(atlas.vertices.size(), mesh.vertices.size());

    for (std::size_t i = 0; i < sizes.size(); i++)
    {
      const LightmapAtlasBlock &block = atlas.blocks[i];
      EXPECT_TRUE(block.is_lit);
      EXPECT_EQ(block.width, sizes[i].first);
      EXPECT_EQ(block.height, sizes[i].second);

      // the block and its border lie inside the page
      EXPECT_GE(block.x, 1u);
      EXPECT_GE(block.y, 1u);
      EXPECT_LE(block.x + block.width + 1, atlas.side);
      EXPECT_LE(block.y + block.height + 1, atlas.side);

      // every sample is where the block says
      for (std::uint32_t row = 0; row < block.height; row++)
      {
        for (std::uint32_t column = 0; column < block.width; column++)
        {
          EXPECT_EQ(PixelAt(atlas.pages[0], block.x + column, block.y + row),
            mesh.faces[i].lightmap[row * block.width + column]) << "face " << i;
        }
      }

      // no other block reaches into it or into its border
      for (std::size_t j = i + 1; j < sizes.size(); j++)
      {
        const LightmapAtlasBlock &other = atlas.blocks[j];
        const bool apart =
          block.x + block.width + 1 <= other.x - 1 || other.x + other.width + 1 <= block.x - 1 ||
          block.y + block.height + 1 <= other.y - 1 || other.y + other.height + 1 <= block.y - 1;
        EXPECT_TRUE(apart) << "faces " << i << " and " << j;
      }

      // the corners of the face find its own outermost samples
      const std::vector<std::uint8_t> &samples = mesh.faces[i].lightmap;
      const std::uint32_t first = mesh.faces[i].first_vertex;
      EXPECT_EQ(SampleAt(atlas, atlas.vertices[first]), samples[0]);
      EXPECT_EQ(SampleAt(atlas, atlas.vertices[first + 1]), samples[block.width - 1]);
      EXPECT_EQ(SampleAt(atlas, atlas.vertices[first + 2]), samples.back());
      EXPECT_EQ(SampleAt(atlas, atlas.vertices[first + 3]), samples[samples.size() - block.width]);
    }
  }

  TEST(LightmapAtlasTest, PutsTheMiddleOfEverySampleOfAFaceOnTheMiddleOfItsPixel)
  {
    // a vertex on the middle of each of the 4 by 3 samples
    std::vector<std::pair<float, float>> corners;
    for (int row = 0; row < 3; row++)
    {
      for (int column = 0; column < 4; column++)
      {
        corners.emplace_back((static_cast<float>(column) + 0.5f) / 4.0f, (static_cast<float>(row) + 0.5f) / 3.0f);
      }
    }

    BspMesh mesh;
    // another face first, so that the one that is looked at is not at the corner of the page
    AddFace(mesh, 6, 5, CountUp(6, 5, 200));
    AddFace(mesh, 4, 3, CountUp(4, 3, 100), corners);

    LightmapAtlas atlas;
    std::string error;
    ASSERT_TRUE(atlas.Build(mesh, error)) << error;

    const LightmapAtlasBlock &block = atlas.blocks[1];
    EXPECT_GT(block.x, 1u);

    const auto side = static_cast<float>(atlas.side);
    for (std::uint32_t row = 0; row < 3; row++)
    {
      for (std::uint32_t column = 0; column < 4; column++)
      {
        const LightmapAtlasVertex &vertex = atlas.vertices[mesh.faces[1].first_vertex + row * 4 + column];
        EXPECT_EQ(SampleAt(atlas, vertex), 100 + row * 4 + column) << "sample " << column << ", " << row;
        EXPECT_FLOAT_EQ(vertex.u, (static_cast<float>(block.x + column) + 0.5f) / side);
        EXPECT_FLOAT_EQ(vertex.v, (static_cast<float>(block.y + row) + 0.5f) / side);
      }
    }
  }

  TEST(LightmapAtlasTest, PutsTheBordersOfTheLightmapOfAFaceOnTheEdgesOfItsBlock)
  {
    BspMesh mesh;
    AddFace(mesh, 4, 2, CountUp(4, 2, 0), {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.25f, 0.5f}});

    LightmapAtlas atlas;
    std::string error;
    ASSERT_TRUE(atlas.Build(mesh, error)) << error;
    ASSERT_EQ(atlas.side, 8u);
    ASSERT_EQ(atlas.blocks[0].x, 1u);
    ASSERT_EQ(atlas.blocks[0].y, 1u);

    // u = (x + lightmap_u * width) / side, and the same downwards
    EXPECT_FLOAT_EQ(atlas.vertices[0].u, 1.0f / 8.0f);
    EXPECT_FLOAT_EQ(atlas.vertices[0].v, 1.0f / 8.0f);
    EXPECT_FLOAT_EQ(atlas.vertices[1].u, 5.0f / 8.0f);
    EXPECT_FLOAT_EQ(atlas.vertices[2].v, 3.0f / 8.0f);
    EXPECT_FLOAT_EQ(atlas.vertices[3].u, 2.0f / 8.0f);
    EXPECT_FLOAT_EQ(atlas.vertices[3].v, 2.0f / 8.0f);
  }

  TEST(LightmapAtlasTest, TakesTheSmallestPageThatHoldsEverythingAndTheLargestSideWhenNoneDoes)
  {
    BspMesh tiny;
    AddFace(tiny, 2, 2, CountUp(2, 2, 0));

    LightmapAtlas atlas;
    std::string error;
    ASSERT_TRUE(atlas.Build(tiny, error)) << error;
    EXPECT_EQ(atlas.side, 4u);
    EXPECT_EQ(atlas.pages.size(), 1u);

    // 8 by 8 samples and a border are 10 by 10 pixels: the powers of two
    // below are too small, and the largest side need not be one
    BspMesh mesh;
    AddFace(mesh, 8, 8, CountUp(8, 8, 0));
    ASSERT_TRUE(atlas.Build(mesh, error, 10)) << error;
    EXPECT_EQ(atlas.side, 10u);
    EXPECT_EQ(atlas.pages.size(), 1u);
    EXPECT_EQ(atlas.pages[0].pixels.size(), 10u * 10u * 4u);
    EXPECT_EQ(SampleAt(atlas, atlas.vertices[2]), 63);
  }

  TEST(LightmapAtlasTest, GoesOnWithAnotherPageWhenOneIsFull)
  {
    // a page of 16 by 16 pixels holds four blocks of 6 by 6 samples with
    // their borders, so ten of them need three pages
    BspMesh mesh;
    for (int i = 0; i < 10; i++) { AddFace(mesh, 6, 6, CountUp(6, 6, i * 20)); }

    LightmapAtlas atlas;
    std::string error;
    ASSERT_TRUE(atlas.Build(mesh, error, 16)) << error;

    EXPECT_EQ(atlas.side, 16u);
    ASSERT_EQ(atlas.pages.size(), 3u);
    for (const LightmapAtlasPage &page : atlas.pages)
    {
      EXPECT_EQ(page.width, 16u);
      EXPECT_EQ(page.height, 16u);
      EXPECT_EQ(page.pixels.size(), 16u * 16u * 4u);
    }

    std::vector<int> faces_on_page(3, 0);
    for (std::size_t i = 0; i < mesh.faces.size(); i++)
    {
      const LightmapAtlasBlock &block = atlas.blocks[i];
      ASSERT_LT(block.page, 3u);
      faces_on_page[block.page]++;

      // all corners of a face are on the page of its block, and find its samples there
      const std::uint32_t first = mesh.faces[i].first_vertex;
      for (std::uint32_t corner = 0; corner < 4; corner++)
      {
        EXPECT_EQ(atlas.vertices[first + corner].page, block.page);
      }
      EXPECT_EQ(SampleAt(atlas, atlas.vertices[first]), i * 20);
      EXPECT_EQ(SampleAt(atlas, atlas.vertices[first + 2]), i * 20 + 35);
    }
    EXPECT_THAT(faces_on_page, ElementsAre(4, 4, 2));
  }

  TEST(LightmapAtlasTest, PutsFacesWithoutALightmapOnABlockThatIsFullyBright)
  {
    BspMesh mesh;
    // the sky has no size, and a wall the level gives no light has one
    AddFace(mesh, 0, 0, {});
    AddFace(mesh, 4, 4, CountUp(4, 4, 0));
    AddFace(mesh, 3, 2, {}, {{0.1f, 0.2f}, {0.9f, 0.2f}, {0.9f, 0.8f}});

    LightmapAtlas atlas;
    std::string error;
    ASSERT_TRUE(atlas.Build(mesh, error)) << error;
    ASSERT_EQ(atlas.pages.size(), 1u);
    ASSERT_EQ(atlas.vertices.size(), 11u);

    EXPECT_TRUE(atlas.blocks[1].is_lit);
    for (const std::size_t face : {0u, 2u})
    {
      const LightmapAtlasBlock &block = atlas.blocks[face];
      EXPECT_FALSE(block.is_lit);
      EXPECT_EQ(block.page, 0u);
      EXPECT_EQ(block.width, 1u);
      EXPECT_EQ(block.height, 1u);
      EXPECT_EQ(block.x, atlas.blocks[0].x);
      EXPECT_EQ(block.y, atlas.blocks[0].y);

      // the pixel and its border are bright
      for (std::uint32_t y = block.y - 1; y <= block.y + 1; y++)
      {
        for (std::uint32_t x = block.x - 1; x <= block.x + 1; x++)
        {
          EXPECT_EQ(PixelAt(atlas.pages[0], x, y), LightmapAtlas::fully_bright);
        }
      }

      // every corner lies on the middle of the pixel
      for (std::uint32_t corner = 0; corner < mesh.faces[face].vertex_count; corner++)
      {
        const LightmapAtlasVertex &vertex = atlas.vertices[mesh.faces[face].first_vertex + corner];
        EXPECT_EQ(vertex.page, 0u);
        EXPECT_EQ(SampleAt(atlas, vertex), 255);
        EXPECT_FLOAT_EQ(vertex.u, (static_cast<float>(block.x) + 0.5f) / static_cast<float>(atlas.side));
        EXPECT_FLOAT_EQ(vertex.v, (static_cast<float>(block.y) + 0.5f) / static_cast<float>(atlas.side));
      }
    }

    // the lit face between them is not disturbed
    EXPECT_EQ(SampleAt(atlas, atlas.vertices[4]), 0);
    EXPECT_EQ(SampleAt(atlas, atlas.vertices[6]), 15);
  }

  TEST(LightmapAtlasTest, MakesOneBrightPageOfAMeshWithoutAnyLightmap)
  {
    BspMesh mesh;
    AddFace(mesh, 5, 5, {});

    LightmapAtlas atlas;
    std::string error;
    ASSERT_TRUE(atlas.Build(mesh, error)) << error;

    EXPECT_EQ(atlas.side, LightmapAtlas::smallest_side);
    ASSERT_EQ(atlas.pages.size(), 1u);
    EXPECT_THAT(atlas.vertices, Each(::testing::Field(&LightmapAtlasVertex::page, 0u)));
    for (const LightmapAtlasVertex &vertex : atlas.vertices) { EXPECT_EQ(SampleAt(atlas, vertex), 255); }
  }

  TEST(LightmapAtlasTest, TakesOnlyTheFirstLightmapOfAFaceWithSeveralStyles)
  {
    BspMesh mesh;
    BspMeshFace &face = AddFace(mesh, 2, 1, {10, 20, 200, 210});
    face.light_styles = {0, 3, 255, 255};

    LightmapAtlas atlas;
    std::string error;
    ASSERT_TRUE(atlas.Build(mesh, error)) << error;

    EXPECT_EQ(PixelAt(atlas.pages[0], 0, 1), 10);
    EXPECT_EQ(PixelAt(atlas.pages[0], 1, 1), 10);
    EXPECT_EQ(PixelAt(atlas.pages[0], 2, 1), 20);
    EXPECT_EQ(PixelAt(atlas.pages[0], 3, 1), 20);
  }

  TEST(LightmapAtlasTest, MakesNothingOfAMeshWithoutFaces)
  {
    LightmapAtlas atlas;
    std::string error;
    ASSERT_TRUE(atlas.Build(BspMesh{}, error)) << error;

    EXPECT_EQ(atlas.side, 0u);
    EXPECT_THAT(atlas.pages, IsEmpty());
    EXPECT_THAT(atlas.vertices, IsEmpty());
    EXPECT_THAT(atlas.blocks, IsEmpty());
  }

  TEST(LightmapAtlasTest, RefusesAFaceThatIsLargerThanAPage)
  {
    // 6 samples and a border fill a page of 8 pixels, 7 do not
    BspMesh fits;
    AddFace(fits, 6, 6, CountUp(6, 6, 0));
    EXPECT_EQ(ReasonOfRefusal(fits, 8), "");

    BspMesh wide;
    AddFace(wide, 2, 2, CountUp(2, 2, 0));
    AddFace(wide, 7, 1, CountUp(7, 1, 0));
    EXPECT_EQ(ReasonOfRefusal(wide, 8),
      "face 1 has a lightmap of 7 by 1 samples, which with its border is larger than a page of 8 by 8 pixels");

    BspMesh high;
    AddFace(high, 1, 7, CountUp(1, 7, 0));
    EXPECT_EQ(ReasonOfRefusal(high, 8),
      "face 0 has a lightmap of 1 by 7 samples, which with its border is larger than a page of 8 by 8 pixels");
  }

  TEST(LightmapAtlasTest, RefusesALightmapWhoseBytesAreNotWhatItsSizeAsksFor)
  {
    BspMesh too_few;
    AddFace(too_few, 3, 2, {1, 2, 3, 4, 5});
    EXPECT_EQ(ReasonOfRefusal(too_few), "face 0 has 1 lightmaps of 3 by 2 samples, and 5 bytes of them");

    BspMesh too_many;
    AddFace(too_many, 3, 2, {1, 2, 3, 4, 5, 6, 7});
    EXPECT_EQ(ReasonOfRefusal(too_many), "face 0 has 1 lightmaps of 3 by 2 samples, and 7 bytes of them");

    // the bytes of one lightmap where the styles ask for two
    BspMesh one_of_two;
    AddFace(one_of_two, 3, 2, {1, 2, 3, 4, 5, 6}).light_styles = {0, 1, 255, 255};
    EXPECT_EQ(ReasonOfRefusal(one_of_two), "face 0 has 2 lightmaps of 3 by 2 samples, and 6 bytes of them");

    BspMesh no_style;
    AddFace(no_style, 3, 2, {1, 2, 3, 4, 5, 6}).light_styles = {255, 255, 255, 255};
    EXPECT_EQ(ReasonOfRefusal(no_style), "face 0 has 0 lightmaps of 3 by 2 samples, and 6 bytes of them");

    BspMesh no_size;
    AddFace(no_size, 0, 0, {1});
    EXPECT_EQ(ReasonOfRefusal(no_size), "face 0 has 1 lightmaps of 0 by 0 samples, and 1 bytes of them");
  }

  TEST(LightmapAtlasTest, RefusesAFaceWhoseVerticesAreNotThere)
  {
    BspMesh mesh;
    AddFace(mesh, 2, 2, CountUp(2, 2, 0));
    mesh.faces[0].vertex_count = 5;
    EXPECT_EQ(ReasonOfRefusal(mesh), "face 0 names 5 vertices from 0, and there are 4");

    mesh.faces[0].vertex_count = 1;
    mesh.faces[0].first_vertex = 4;
    EXPECT_EQ(ReasonOfRefusal(mesh), "face 0 names 1 vertices from 4, and there are 4");
  }

  TEST(LightmapAtlasTest, RefusesALargestSideThatHoldsNothing)
  {
    BspMesh mesh;
    AddFace(mesh, 1, 1, {7});
    EXPECT_EQ(ReasonOfRefusal(mesh, 3), "a page may be 3 pixels wide and high, fewer than 4");
    EXPECT_EQ(ReasonOfRefusal(mesh, 0), "a page may be 0 pixels wide and high, fewer than 4");
    EXPECT_EQ(ReasonOfRefusal(mesh, 4), "");
  }

  TEST(LightmapAtlasTest, StaysAsItWasWhenItRefuses)
  {
    BspMesh good;
    AddFace(good, 2, 2, {1, 2, 3, 4});

    LightmapAtlas atlas;
    std::string error;
    ASSERT_TRUE(atlas.Build(good, error)) << error;

    BspMesh bad;
    AddFace(bad, 2, 2, {1, 2, 3});
    EXPECT_FALSE(atlas.Build(bad, error));

    EXPECT_EQ(atlas.side, 4u);
    ASSERT_EQ(atlas.pages.size(), 1u);
    ASSERT_EQ(atlas.vertices.size(), 4u);
    EXPECT_EQ(SampleAt(atlas, atlas.vertices[2]), 4);
  }

  TEST(LightmapAtlasTest, PacksTheLightmapsOfRealLevelsWhenTheyAreThere)
  {
    const std::filesystem::path maps = std::filesystem::path(QUAKE_TEST_DATA_DIRECTORY) / "maps";

    std::size_t packed = 0;
    std::error_code ignored;
    for (std::filesystem::directory_iterator entry(maps, ignored), end; !ignored && entry != end;
      entry.increment(ignored))
    {
      if (entry->path().extension() != ".bsp") { continue; }

      std::ifstream stream(entry->path(), std::ios::binary);
      const std::vector<std::uint8_t> bytes(
        (std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

      // a level of another version is not what is read here
      BspFile file;
      std::string error;
      if (!file.Read(bytes, error)) { continue; }

      BspMesh mesh;
      ASSERT_TRUE(mesh.Build(file, 0, error)) << entry->path().string() << ": " << error;

      LightmapAtlas atlas;
      ASSERT_TRUE(atlas.Build(mesh, error)) << entry->path().string() << ": " << error;

      EXPECT_LE(atlas.side, LightmapAtlas::default_largest_side);
      EXPECT_FALSE(atlas.pages.empty());
      for (const LightmapAtlasPage &page : atlas.pages)
      {
        EXPECT_EQ(page.width, atlas.side);
        EXPECT_EQ(page.height, atlas.side);
        EXPECT_EQ(page.pixels.size(), static_cast<std::size_t>(atlas.side) * atlas.side * 4);
      }

      ASSERT_EQ(atlas.vertices.size(), mesh.vertices.size());
      ASSERT_EQ(atlas.blocks.size(), mesh.faces.size());
      std::size_t outside = 0;
      for (const LightmapAtlasVertex &vertex : atlas.vertices)
      {
        if (!(vertex.u >= 0.0f && vertex.u <= 1.0f && vertex.v >= 0.0f && vertex.v <= 1.0f) ||
          vertex.page >= atlas.pages.size())
        {
          outside++;
        }
      }
      EXPECT_EQ(outside, 0u) << entry->path().string();

      // every corner of a face lies on the page of its block, and within
      // the block, the width of a hair allowed for
      std::size_t astray = 0;
      for (std::size_t i = 0; i < mesh.faces.size(); i++)
      {
        const LightmapAtlasBlock &block = atlas.blocks[i];
        const auto side = static_cast<float>(atlas.side);
        for (std::uint32_t corner = 0; corner < mesh.faces[i].vertex_count; corner++)
        {
          const LightmapAtlasVertex &vertex = atlas.vertices[mesh.faces[i].first_vertex + corner];
          const float x = vertex.u * side;
          const float y = vertex.v * side;
          if (vertex.page != block.page ||
            x < static_cast<float>(block.x) - 0.01f || x > static_cast<float>(block.x + block.width) + 0.01f ||
            y < static_cast<float>(block.y) - 0.01f || y > static_cast<float>(block.y + block.height) + 0.01f)
          {
            astray++;
          }
        }
      }
      EXPECT_EQ(astray, 0u) << entry->path().string();

      packed++;
    }
    if (packed == 0) { GTEST_SKIP() << "No compiled level of version 29 in " << maps.string(); }
  }
}
