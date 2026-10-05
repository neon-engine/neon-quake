#include "lightmap-atlas.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

namespace quake
{
  // Helpers of LightmapAtlas: what is packed, the packer that fills pages
  // shelf after shelf, and the sentence that refuses a face.
  namespace
  {
    /// A rectangle of pixels to find a place for: the lightmap of a face
    /// with its border, or the fully bright block.
    struct Item
    {
      /// Stands for the fully bright block in place of the number of a face.
      static constexpr std::size_t no_face = static_cast<std::size_t>(-1);

      std::size_t face = no_face;

      /// Its size with the border.
      std::uint32_t width = 0;
      std::uint32_t height = 0;

      /// Whether a light of it flickers, pulses, or is switched: whether it
      /// has a style that is not 0, and so is composed anew while the game
      /// runs.
      bool changes = false;

      /// The page it was put on, and the pixel its border starts at.
      std::uint32_t page = 0;
      std::uint32_t x = 0;
      std::uint32_t y = 0;
    };

    /// Puts the items on square pages of a side, in the order they come in,
    /// which is best from the highest to the lowest. A page is filled in
    /// shelves: items go next to each other from the left until one does not
    /// fit, which starts the next shelf below, as high as its highest item;
    /// and a shelf that does not fit below starts the next page. Returns how
    /// many pages it took, or 0 when an item is larger than a page.
    std::uint32_t pack(std::vector<Item> &items, const std::uint32_t side)
    {
      std::uint32_t page = 0;
      std::uint32_t x = 0;
      std::uint32_t y = 0;
      std::uint32_t shelf_height = 0;

      for (Item &item : items)
      {
        if (item.width > side || item.height > side) { return 0; }

        if (item.width > side - x)
        {
          x = 0;
          y += shelf_height;
          shelf_height = 0;
        }
        if (item.height > side - y)
        {
          page++;
          x = 0;
          y = 0;
          shelf_height = 0;
        }
        // The first item of a shelf is its highest where they come sorted.
        // Where the order starts anew from the highest, in the middle of a
        // shelf, the shelf grows to hold it.
        shelf_height = std::max(shelf_height, item.height);

        item.page = page;
        item.x = x;
        item.y = y;
        x += item.width;
      }
      return page + 1;
    }

    /// What each style is worth in 256ths, which is what the pixels are
    /// counted with: whole numbers, as the original counts them.
    using Scales = std::array<std::uint32_t, LightStyles::count>;

    /// A value in 256ths. Nothing below 0, which also takes what is not a
    /// number, and nothing so large that a sum of four could overflow.
    std::uint32_t scale_of(const float value)
    {
      if (!(value > 0.0f)) { return 0; }
      return static_cast<std::uint32_t>(std::min(value, 256.0f) * 256.0f + 0.5f);
    }

    Scales scales_of(const LightStyles::Values &values)
    {
      Scales scales{};
      for (std::size_t style = 0; style < scales.size(); style++) { scales[style] = scale_of(values[style]); }
      return scales;
    }

    /// Makes the pixels of the block of a lit face, and of the border
    /// around it, from its samples. Every pixel takes the sample nearest to
    /// it: a pixel of the block its own, a pixel of the border the one at
    /// the edge next to it.
    void compose(LightmapAtlas &atlas, const LightmapAtlasBlock &block, const Scales &scales)
    {
      constexpr std::uint32_t border = LightmapAtlas::border;
      LightmapAtlasPage &page = atlas.pages[block.page];

      const std::size_t style_count = block.CountStyles();
      const std::size_t layer_size = static_cast<std::size_t>(block.width) * block.height * 3;
      std::array<std::uint32_t, 4> scale_of_layer{};
      for (std::size_t layer = 0; layer < style_count; layer++)
      {
        const std::uint8_t style = block.styles[layer];
        scale_of_layer[layer] = style < scales.size() ? scales[style] : 256;
      }

      for (std::uint32_t row = 0; row < block.height + 2 * border; row++)
      {
        const std::uint32_t sample_row = std::clamp(row, border, block.height + border - 1) - border;
        for (std::uint32_t column = 0; column < block.width + 2 * border; column++)
        {
          const std::uint32_t sample_column = std::clamp(column, border, block.width + border - 1) - border;
          const std::size_t sample =
            block.first_sample + (static_cast<std::size_t>(sample_row) * block.width + sample_column) * 3;

          std::array<std::uint32_t, 3> sum{};
          for (std::size_t layer = 0; layer < style_count; layer++)
          {
            const std::uint8_t *colour = &atlas.samples[sample + layer * layer_size];
            for (std::size_t part = 0; part < 3; part++) { sum[part] += colour[part] * scale_of_layer[layer]; }
          }

          const std::size_t at =
            (static_cast<std::size_t>(block.y - border + row) * page.width + (block.x - border + column)) * 4;
          for (std::size_t part = 0; part < 3; part++)
          {
            page.pixels[at + part] = static_cast<std::uint8_t>(std::min<std::uint32_t>(sum[part] >> 8, 255));
          }
          page.pixels[at + 3] = 255;
        }
      }
    }

    bool refuse(std::string &error, const std::size_t face, const std::string &reason)
    {
      error = "face " + std::to_string(face) + " " + reason;
      return false;
    }
  }

  bool LightmapAtlas::Build(
    const BspMesh &mesh,
    std::string &error,
    const std::uint32_t largest_side,
    const LitFile *lit)
  {
    if (largest_side < smallest_side)
    {
      error = "a page may be " + std::to_string(largest_side) + " pixels wide and high, fewer than " +
        std::to_string(smallest_side);
      return false;
    }

    // what there is to pack: the lightmap of every face that has one, and
    // the fully bright block when a face has none
    std::vector<Item> items;
    items.reserve(mesh.faces.size() + 1);
    bool has_unlit_face = false;

    for (std::size_t face_index = 0; face_index < mesh.faces.size(); face_index++)
    {
      const BspMeshFace &face = mesh.faces[face_index];

      if (face.first_vertex > mesh.vertices.size() || face.vertex_count > mesh.vertices.size() - face.first_vertex)
      {
        return refuse(error, face_index, "names " + std::to_string(face.vertex_count) + " vertices from " +
          std::to_string(face.first_vertex) + ", and there are " + std::to_string(mesh.vertices.size()));
      }

      if (face.lightmap.empty())
      {
        has_unlit_face = true;
        continue;
      }

      const std::uint64_t expected = static_cast<std::uint64_t>(face.lightmap_width) *
        static_cast<std::uint64_t>(face.lightmap_height) * face.CountLightStyles();
      if (expected != face.lightmap.size())
      {
        return refuse(error, face_index, "has " + std::to_string(face.CountLightStyles()) + " lightmaps of " +
          std::to_string(face.lightmap_width) + " by " + std::to_string(face.lightmap_height) +
          " samples, and " + std::to_string(face.lightmap.size()) + " bytes of them");
      }

      if (face.lightmap_width > largest_side - 2 * border || face.lightmap_height > largest_side - 2 * border)
      {
        return refuse(error, face_index, "has a lightmap of " + std::to_string(face.lightmap_width) + " by " +
          std::to_string(face.lightmap_height) + " samples, which with its border is larger than a page of " +
          std::to_string(largest_side) + " by " + std::to_string(largest_side) + " pixels");
      }

      // the colours of a face lie where its bytes lie in the lighting
      if (lit != nullptr && (face.light_offset < 0 ||
        static_cast<std::uint64_t>(face.light_offset) > lit->GetSampleCount() ||
        expected > lit->GetSampleCount() - static_cast<std::uint64_t>(face.light_offset)))
      {
        return refuse(error, face_index, "has " + std::to_string(expected) + " samples from " +
          std::to_string(face.light_offset) + " of the lighting, and the coloured light has " +
          std::to_string(lit->GetSampleCount()));
      }

      Item item;
      item.face = face_index;
      item.width = face.lightmap_width + 2 * border;
      item.height = face.lightmap_height + 2 * border;
      item.changes = std::ranges::any_of(face.light_styles, [](const std::uint8_t style)
      {
        return style != 0 && style != BspFace::no_style;
      });
      items.push_back(item);
    }

    if (has_unlit_face)
    {
      Item item;
      item.width = 1 + 2 * border;
      item.height = 1 + 2 * border;
      items.push_back(item);
    }

    // The faces whose light changes first, so that they lie together on as
    // few pages as they take: a page with one of them is handed to the
    // renderer again every time a style ticks, ten times a second, and a
    // level has few of them among thousands. Then the highest first, so
    // that a shelf wastes little above its items. What is as high stays in
    // the order of the faces, which keeps the atlas of a level the same
    // every time it is made.
    std::ranges::stable_sort(items, [](const Item &a, const Item &b)
    {
      return a.changes != b.changes ? a.changes : a.height > b.height;
    });

    // the smallest power of two that holds everything on one page, or the
    // largest side with as many pages as it takes
    std::uint32_t page_side = 0;
    std::uint32_t page_count = 0;
    if (!items.empty())
    {
      for (std::uint64_t trial = smallest_side; trial < largest_side; trial *= 2)
      {
        if (pack(items, static_cast<std::uint32_t>(trial)) == 1)
        {
          page_side = static_cast<std::uint32_t>(trial);
          page_count = 1;
          break;
        }
      }
      if (page_count == 0)
      {
        page_side = largest_side;
        page_count = pack(items, largest_side);
      }
    }

    LightmapAtlas atlas;
    atlas.side = page_side;
    atlas.vertices.resize(mesh.vertices.size());
    atlas.blocks.resize(mesh.faces.size());

    atlas.pages.resize(page_count);
    for (LightmapAtlasPage &page : atlas.pages)
    {
      page.width = page_side;
      page.height = page_side;
      page.pixels.resize(static_cast<std::size_t>(page_side) * page_side * 4);
      // black, and solid as every pixel is
      for (std::size_t at = 3; at < page.pixels.size(); at += 4) { page.pixels[at] = 255; }
    }

    atlas.values.fill(1.0f);
    const Scales scales = scales_of(atlas.values);

    LightmapAtlasBlock bright_block;
    for (const Item &item : items)
    {
      LightmapAtlasPage &page = atlas.pages[item.page];

      if (item.face == Item::no_face)
      {
        for (std::uint32_t row = 0; row < item.height; row++)
        {
          for (std::uint32_t column = 0; column < item.width; column++)
          {
            const std::size_t at = (static_cast<std::size_t>(item.y + row) * page_side + item.x + column) * 4;
            page.pixels[at] = page.pixels[at + 1] = page.pixels[at + 2] = fully_bright;
          }
        }
        bright_block.page = item.page;
        bright_block.x = item.x + border;
        bright_block.y = item.y + border;
        bright_block.width = 1;
        bright_block.height = 1;
        continue;
      }

      const BspMeshFace &face = mesh.faces[item.face];

      LightmapAtlasBlock &block = atlas.blocks[item.face];
      block.page = item.page;
      block.x = item.x + border;
      block.y = item.y + border;
      block.width = face.lightmap_width;
      block.height = face.lightmap_height;
      block.is_lit = true;
      block.styles = face.light_styles;
      block.first_sample = atlas.samples.size();

      // the samples of all its lightmaps: the colours of the coloured light
      // when there is one, and the brightness of the level three times over
      // when not
      if (lit != nullptr)
      {
        const auto start = lit->GetColours().begin() + static_cast<std::ptrdiff_t>(face.light_offset) * 3;
        atlas.samples.insert(atlas.samples.end(), start, start + static_cast<std::ptrdiff_t>(face.lightmap.size()) * 3);
      } else
      {
        for (const std::uint8_t sample : face.lightmap) { atlas.samples.insert(atlas.samples.end(), 3, sample); }
      }

      for (std::size_t layer = 0; layer < block.CountStyles(); layer++) { atlas.styles.push_back(block.styles[layer]); }

      compose(atlas, block, scales);
    }

    std::ranges::sort(atlas.styles);
    atlas.styles.erase(std::ranges::unique(atlas.styles).begin(), atlas.styles.end());

    for (std::size_t face_index = 0; face_index < mesh.faces.size(); face_index++)
    {
      const BspMeshFace &face = mesh.faces[face_index];
      LightmapAtlasBlock &block = atlas.blocks[face_index];

      if (!block.is_lit)
      {
        // all its vertices lie on the middle of the one pixel of the bright
        // block, wherever they lie on the face
        block = bright_block;
        for (std::uint32_t corner = 0; corner < face.vertex_count; corner++)
        {
          LightmapAtlasVertex &vertex = atlas.vertices[face.first_vertex + corner];
          vertex.u = static_cast<float>((block.x + 0.5) / page_side);
          vertex.v = static_cast<float>((block.y + 0.5) / page_side);
          vertex.page = block.page;
        }
        continue;
      }

      // 0 and 1 of the face are the borders of its block, so the middle of
      // sample i, at (i + 0.5) / width, lands on the middle of pixel x + i
      for (std::uint32_t corner = 0; corner < face.vertex_count; corner++)
      {
        const BspMeshVertex &source = mesh.vertices[face.first_vertex + corner];
        LightmapAtlasVertex &vertex = atlas.vertices[face.first_vertex + corner];
        vertex.u = static_cast<float>(
          (block.x + static_cast<double>(source.lightmap_u) * block.width) / page_side);
        vertex.v = static_cast<float>(
          (block.y + static_cast<double>(source.lightmap_v) * block.height) / page_side);
        vertex.page = block.page;
      }
    }

    *this = std::move(atlas);
    return true;
  }

  std::vector<std::uint32_t> LightmapAtlas::Compose(const LightStyles::Values &new_values)
  {
    const Scales old_scales = scales_of(values);
    const Scales new_scales = scales_of(new_values);
    values = new_values;

    // most of the time no style is worth another value than before
    std::array<bool, LightStyles::count> is_changed{};
    bool any_changed = false;
    for (std::size_t style = 0; style < new_scales.size(); style++)
    {
      is_changed[style] = old_scales[style] != new_scales[style];
      any_changed = any_changed || is_changed[style];
    }
    if (!any_changed) { return {}; }

    std::vector<bool> page_changed(pages.size(), false);
    for (const LightmapAtlasBlock &block : blocks)
    {
      if (!block.is_lit || block.page >= pages.size()) { continue; }

      bool face_changed = false;
      for (std::size_t layer = 0; layer < block.CountStyles(); layer++)
      {
        const std::uint8_t style = block.styles[layer];
        face_changed = face_changed || (style < is_changed.size() && is_changed[style]);
      }
      if (!face_changed) { continue; }

      compose(*this, block, new_scales);
      page_changed[block.page] = true;
    }

    std::vector<std::uint32_t> changed;
    for (std::uint32_t page = 0; page < page_changed.size(); page++)
    {
      if (page_changed[page]) { changed.push_back(page); }
    }
    return changed;
  }
} // quake
