#include "lightmap-atlas.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <map>
#include <span>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "bsp-file.hpp"
#include "real-data.test.hpp"

namespace
{
  using quake::BspFile;
  using quake::BspMesh;
  using quake::BspMeshFace;
  using quake::LightmapAtlas;
  using quake::LightmapAtlasBlock;
  using quake::LightmapAtlasPage;
  using quake::LightStyles;
  using quake::LitFile;
  using quake::RealData;

  /// How many samples of the faces of a mesh are not shown in an atlas as
  /// the sum of their lightmaps, each multiplied by what its style is worth
  /// and no brighter than a byte holds. `colours` are three bytes for every
  /// byte of the lighting of the level, or empty for grey light. `leeway`
  /// is how far a pixel may be from the sum: none when every value is a
  /// whole number, and a little when not, since the atlas counts a value in
  /// 256ths and drops what is left over of a sum.
  std::size_t CountWrongSamples(
    const BspMesh &mesh,
    const LightmapAtlas &atlas,
    const std::span<const std::uint8_t> colours,
    const LightStyles::Values &values,
    const float leeway)
  {
    std::size_t wrong = 0;
    for (std::size_t i = 0; i < mesh.faces.size(); i++)
    {
      const BspMeshFace &face = mesh.faces[i];
      const LightmapAtlasBlock &block = atlas.blocks[i];
      if (!block.is_lit) { continue; }

      const LightmapAtlasPage &page = atlas.pages[block.page];
      const std::size_t layer_size = static_cast<std::size_t>(block.width) * block.height;
      for (std::size_t sample = 0; sample < layer_size; sample++)
      {
        std::array<float, 3> sum{};
        for (std::size_t layer = 0; layer < face.CountLightStyles(); layer++)
        {
          const std::size_t at = layer * layer_size + sample;
          const float value = values[face.light_styles[layer]];
          for (std::size_t part = 0; part < 3; part++)
          {
            const std::uint8_t byte = colours.empty() ?
              face.lightmap[at] : colours[(static_cast<std::size_t>(face.light_offset) + at) * 3 + part];
            sum[part] += static_cast<float>(byte) * value;
          }
        }

        const std::size_t x = block.x + sample % block.width;
        const std::size_t y = block.y + sample / block.width;
        const std::uint8_t *pixel = &page.pixels[(y * page.width + x) * 4];
        for (std::size_t part = 0; part < 3; part++)
        {
          const float expected = std::min(sum[part], 255.0f);
          if (std::abs(static_cast<float>(pixel[part]) - expected) > leeway + 0.01f)
          {
            wrong++;
          }
        }
      }
    }
    return wrong;
  }

  TEST(LightmapAtlasRealDataTest, PacksAndComposesTheLightOfEveryLevelGreyAndColoured)
  {
    const RealData &data = RealData::Get();
    if (!data.IsThere()) { GTEST_SKIP() << data.GetProblem(); }

    LightStyles::Values ones;
    ones.fill(1.0f);

    // styles as they are in the middle of a game: flickers on their way,
    // and every second switched light off
    LightStyles styles;
    for (std::size_t style = 32; style < 63; style += 2) { styles.Set(style, "a"); }
    const LightStyles::Values later = styles.GetValues(12.75);

    std::size_t levels = 0;
    std::size_t coloured_levels = 0;
    std::size_t lit_faces = 0;
    std::size_t faces_with_several_styles = 0;
    std::map<int, std::size_t> faces_of_style;
    std::map<std::size_t, std::size_t> faces_of_style_count;

    for (const std::string &name : data.ListNames("maps"))
    {
      if (!name.ends_with(".bsp")) { continue; }

      // a level of another version is not what is read here
      BspFile file;
      std::string error;
      if (!file.Read(data.GetBytes(name), error)) { continue; }
      levels++;

      LitFile lit;
      const std::string lit_name = name.substr(0, name.size() - 4) + ".lit";
      const std::span<const std::uint8_t> lit_bytes = data.GetBytes(lit_name);
      const bool is_coloured = !lit_bytes.empty();
      if (is_coloured)
      {
        ASSERT_TRUE(lit.Read(lit_bytes, file.lighting.size(), error)) << lit_name << ": " << error;
        coloured_levels++;
      }

      for (std::size_t model = 0; model < file.models.size(); model++)
      {
        const std::string where = name + " model " + std::to_string(model);

        BspMesh mesh;
        ASSERT_TRUE(mesh.Build(file, model, error)) << where << ": " << error;

        for (const BspMeshFace &face : mesh.faces)
        {
          if (face.lightmap.empty()) { continue; }
          lit_faces++;
          faces_of_style_count[face.CountLightStyles()]++;
          if (face.CountLightStyles() > 1) { faces_with_several_styles++; }
          for (std::size_t layer = 0; layer < face.CountLightStyles(); layer++)
          {
            faces_of_style[face.light_styles[layer]]++;
            ASSERT_LT(face.light_styles[layer], LightStyles::count) << where;
          }
        }

        // grey: with every style at 1 a face with only the steady light
        // shows its bytes as they are, and every other the sum
        LightmapAtlas grey;
        ASSERT_TRUE(grey.Build(mesh, error)) << where << ": " << error;
        EXPECT_EQ(CountWrongSamples(mesh, grey, {}, ones, 0.0f), 0u) << where;

        std::size_t changed_bytes = 0;
        for (std::size_t i = 0; i < mesh.faces.size(); i++)
        {
          const BspMeshFace &face = mesh.faces[i];
          const LightmapAtlasBlock &block = grey.blocks[i];
          if (!block.is_lit || face.CountLightStyles() != 1) { continue; }
          for (std::size_t sample = 0; sample < face.lightmap.size(); sample++)
          {
            const std::size_t x = block.x + sample % block.width;
            const std::size_t y = block.y + sample / block.width;
            const std::uint8_t *pixel = &grey.pages[block.page].pixels[(y * grey.side + x) * 4];
            if (pixel[0] != face.lightmap[sample] || pixel[1] != pixel[0] || pixel[2] != pixel[0] || pixel[3] != 255)
            {
              changed_bytes++;
            }
          }
        }
        EXPECT_EQ(changed_bytes, 0u) << where;

        // the same values again are no work, and others are composed right
        const std::vector<LightmapAtlasPage> built = grey.pages;
        EXPECT_TRUE(grey.Compose(ones).empty()) << where;
        const std::vector<std::uint32_t> changed = grey.Compose(later);
        EXPECT_EQ(CountWrongSamples(mesh, grey, {}, later, 3.0f), 0u) << where;
        for (std::uint32_t page = 0; page < grey.pages.size(); page++)
        {
          if (std::ranges::find(changed, page) == changed.end())
          {
            EXPECT_EQ(grey.pages[page].pixels, built[page].pixels) << where << " page " << page;
          }
        }

        // and back to what was built
        grey.Compose(ones);
        for (std::uint32_t page = 0; page < grey.pages.size(); page++)
        {
          EXPECT_EQ(grey.pages[page].pixels, built[page].pixels) << where << " page " << page;
        }

        if (!is_coloured) { continue; }

        // coloured: packed to the same places, with the colours of the file
        LightmapAtlas coloured;
        ASSERT_TRUE(coloured.Build(mesh, error, LightmapAtlas::default_largest_side, &lit)) << where << ": " << error;
        ASSERT_EQ(coloured.side, grey.side) << where;
        ASSERT_EQ(coloured.pages.size(), grey.pages.size()) << where;
        EXPECT_EQ(CountWrongSamples(mesh, coloured, lit.GetColours(), ones, 0.0f), 0u) << where;
        coloured.Compose(later);
        EXPECT_EQ(CountWrongSamples(mesh, coloured, lit.GetColours(), later, 3.0f), 0u) << where;
      }
    }
    if (levels == 0) { GTEST_SKIP() << "No level of version 29 in the paks"; }

    std::cout << levels << " levels, " << coloured_levels << " with coloured light, " << lit_faces <<
      " faces with a lightmap, " << faces_with_several_styles << " of them with more than one style\n";
    for (const auto &[count, faces] : faces_of_style_count)
    {
      std::cout << "  " << faces << " faces with " << count << " styles\n";
    }
    for (const auto &[style, faces] : faces_of_style) { std::cout << "  style " << style << ": " << faces << " faces\n"; }
  }
}
