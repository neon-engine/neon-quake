#include "lightmap-atlas.hpp"

#include <algorithm>
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

      /// The page it was put on, and the pixel its border starts at.
      std::uint32_t page = 0;
      std::uint32_t x = 0;
      std::uint32_t y = 0;
    };

    /// Puts the items on square pages of a side, in the order they come in,
    /// which has to be from the highest to the lowest. A page is filled in
    /// shelves: items go next to each other from the left until one does not
    /// fit, which starts the next shelf below, as high as its first item;
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
        // the first item of a shelf is its highest, since they come sorted
        if (shelf_height == 0) { shelf_height = item.height; }

        item.page = page;
        item.x = x;
        item.y = y;
        x += item.width;
      }
      return page + 1;
    }

    /// Makes a pixel of a page grey with the brightness of a sample.
    void paint(
      LightmapAtlasPage &page,
      const std::uint32_t x,
      const std::uint32_t y,
      const std::uint8_t sample)
    {
      const std::size_t at = (static_cast<std::size_t>(y) * page.width + x) * 4;
      page.pixels[at] = sample;
      page.pixels[at + 1] = sample;
      page.pixels[at + 2] = sample;
      page.pixels[at + 3] = 255;
    }

    bool refuse(std::string &error, const std::size_t face, const std::string &reason)
    {
      error = "face " + std::to_string(face) + " " + reason;
      return false;
    }
  }

  bool LightmapAtlas::Build(const BspMesh &mesh, std::string &error, const std::uint32_t largest_side)
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

      Item item;
      item.face = face_index;
      item.width = face.lightmap_width + 2 * border;
      item.height = face.lightmap_height + 2 * border;
      items.push_back(item);
    }

    if (has_unlit_face)
    {
      Item item;
      item.width = 1 + 2 * border;
      item.height = 1 + 2 * border;
      items.push_back(item);
    }

    // The highest first, so that a shelf wastes little above its items. What
    // is as high stays in the order of the faces, which keeps the atlas of a
    // level the same every time it is made.
    std::ranges::stable_sort(items, [](const Item &a, const Item &b) { return a.height > b.height; });

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
            paint(page, item.x + column, item.y + row, fully_bright);
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

      // Every pixel of the item takes the sample nearest to it: a pixel of
      // the block its own, a pixel of the border the one at the edge next
      // to it. Only the first lightmap is read, the one of the first style.
      for (std::uint32_t row = 0; row < item.height; row++)
      {
        const std::uint32_t sample_row = std::clamp(row, border, face.lightmap_height + border - 1) - border;
        for (std::uint32_t column = 0; column < item.width; column++)
        {
          const std::uint32_t sample_column = std::clamp(column, border, face.lightmap_width + border - 1) - border;
          const std::size_t sample = static_cast<std::size_t>(sample_row) * face.lightmap_width + sample_column;
          paint(page, item.x + column, item.y + row, face.lightmap[sample]);
        }
      }

      LightmapAtlasBlock &block = atlas.blocks[item.face];
      block.page = item.page;
      block.x = item.x + border;
      block.y = item.y + border;
      block.width = face.lightmap_width;
      block.height = face.lightmap_height;
      block.is_lit = true;
    }

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
} // quake
