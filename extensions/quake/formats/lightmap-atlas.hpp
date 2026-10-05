#ifndef QUAKE_LIGHTMAP_ATLAS_HPP
#define QUAKE_LIGHTMAP_ATLAS_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "bsp-mesh.hpp"
#include "light-styles.hpp"
#include "lightmap-atlas-block.hpp"
#include "lightmap-atlas-page.hpp"
#include "lightmap-atlas-vertex.hpp"
#include "lit-file.hpp"

namespace quake
{
  /// The lightmaps of all faces of a mesh of a level in one picture, or in a
  /// few, and where every vertex of the mesh lies there. A renderer draws a
  /// level with a handful of pictures of light then, not with one for each
  /// of its thousands of faces.
  ///
  /// It is made from a `BspMesh` and keeps nothing of it.
  ///
  /// **Which light.** A face has up to four lightmaps, each of a style: a
  /// light that stays as it is, one that flickers, one the game switches.
  /// What is shown of a face is the sum of its lightmaps, each multiplied by
  /// what its style is worth at the moment, and no brighter than a byte
  /// holds. The atlas keeps the samples of every lightmap, and `Compose`
  /// makes the pixels from them for the values of the styles it is given.
  /// After `Build` the pages show every style at 1, which is all there is
  /// to see of a level whose lights never change.
  ///
  /// **Which colour.** The level says how bright a sample is, and the pages
  /// are grey then. With the `LitFile` of the level, the samples have the
  /// colours of that file instead.
  ///
  /// **How bright a sample is.** The bytes are handed out as the level has
  /// them, and the game doubles them: a sample of `normal_brightness`, 128,
  /// shows the texture as it is, one of 255 nearly twice as bright, and one
  /// of 0 black. Nothing of that is counted in here. Whoever draws decides,
  /// by multiplying the texture by `sample / 128` to look as the game does,
  /// or by `sample / 255` to never go brighter than the texture.
  ///
  /// **Faces without a lightmap**, which are the sky, liquids, and faces the
  /// level gives no light, get a block of their own that is `fully_bright`
  /// all over, on the first page. All their vertices lie on the middle of
  /// it, so that drawing them with the atlas takes nothing from them.
  ///
  /// **The pages** are squares that are all of one size. A side is the
  /// smallest power of two, from `smallest_side` on, that holds everything
  /// on one page, and the largest side that is allowed when none does: the
  /// rest goes on further pages then.
  ///
  /// **Where a face lies.** Its lightmap of `lightmap_width` samples is
  /// copied to a block of as many pixels, at pixel `x` of a page that is
  /// `side` pixels wide. A vertex of the face with `lightmap_u`, where 0 and
  /// 1 are the borders of the lightmap, lies in the atlas at
  ///
  ///     u = (x + lightmap_u * lightmap_width) / side
  ///
  /// and likewise for `v` with `y` and `lightmap_height`. The middle of
  /// sample `i` is at `lightmap_u = (i + 0.5) / lightmap_width`, which
  /// becomes `(x + i + 0.5) / side`: the middle of pixel `x + i`, where that
  /// sample was copied to. So the samples of a face are looked up in the
  /// atlas exactly as in a picture of its own.
  ///
  /// **The border.** Around the block of every face lies a border of one
  /// pixel that repeats the nearest sample of the face, and is no part of
  /// the block: `lightmap_u` of 0 lies on the line between the border and
  /// the first sample. A renderer that blends neighbouring pixels finds the
  /// face itself there, and not the face that happens to be packed next to
  /// it.
  struct LightmapAtlas
  {
    /// The largest side of a page when nothing else is asked for. Every
    /// graphics card draws a picture of that size, a page is four megabytes,
    /// and one page of it holds each of the levels this repository carries,
    /// the largest of which has some twelve thousand faces.
    static constexpr std::uint32_t default_largest_side = 1024;

    /// The smallest side a page is given, and the smallest largest side that
    /// can be asked for: the first power of two that holds the fully bright
    /// block with its border.
    static constexpr std::uint32_t smallest_side = 4;

    /// How many pixels of border lie around the block of a face.
    static constexpr std::uint32_t border = 1;

    /// The sample that shows a texture as it is in the game, which doubles
    /// the light.
    static constexpr std::uint8_t normal_brightness = 128;

    /// The sample of the block for faces without a lightmap.
    static constexpr std::uint8_t fully_bright = 255;

    /// How many pixels every page is wide and high.
    std::uint32_t side = 0;

    /// The pictures. None when the mesh has no faces.
    std::vector<LightmapAtlasPage> pages;

    /// Where each vertex of the mesh lies in the atlas: as many as
    /// `BspMesh::vertices`, in the same order.
    std::vector<LightmapAtlasVertex> vertices;

    /// Where the lightmap of each face was put: as many as `BspMesh::faces`,
    /// in the same order.
    std::vector<LightmapAtlasBlock> blocks;

    /// The samples of all lightmaps of all faces that have one, three bytes
    /// for each: red, green, and blue, which are the same without coloured
    /// light. `LightmapAtlasBlock::first_sample` says where those of a face
    /// start.
    std::vector<std::uint8_t> samples;

    /// The styles that light any face, each once, from the lowest. A host
    /// that finds only 0 here never needs to compose.
    std::vector<std::uint8_t> styles;

    /// What each style was worth when the pages were composed last. All 1
    /// after `Build`.
    LightStyles::Values values{};

    /// How many lightmaps a face has at the most, one for each of its
    /// styles.
    static constexpr std::uint32_t layer_count = 4;

    /// How many pages, the first ones, hold the faces whose light changes:
    /// those with a style that is not 0. They have these pages to
    /// themselves, and every other face has the pages after them. A
    /// renderer that works the light of such a face out as it draws, from
    /// its lightmaps and what its styles are worth at the moment, draws
    /// these pages with `ComposeLayers`, once, and never composes.
    std::uint32_t changing_pages = 0;

    /// How many rows of those pages are used, from the top: as high as a
    /// band of `ComposeLayers` is.
    std::uint32_t changing_rows = 0;

    /// Packs the lightmaps of a mesh, and composes the pages with every
    /// style at 1. `largest_side` is how many pixels a page may be wide and
    /// high at most. `lit` is the coloured light of the level the mesh was
    /// made from, or null for grey light. Returns false, says why in
    /// `error`, and stays as it was when `largest_side` is smaller than
    /// `smallest_side`, when a face names vertices the mesh does not have,
    /// when the lightmap of a face has not the bytes its size and styles ask
    /// for, when a face with its border is larger than a page, or when the
    /// coloured light has not the samples of a face.
    bool Build(
      const BspMesh &mesh,
      std::string &error,
      std::uint32_t largest_side = default_largest_side,
      const LitFile *lit = nullptr);

    /// The lightmaps of the faces of one of the `changing_pages` as they
    /// are, apart, in place of their sum: a picture `side` pixels wide and
    /// `layer_count` bands high, each `changing_rows` rows. Band `k` holds
    /// lightmap `k` of every face of the page, where the page has the face,
    /// with its border, and is black where a face has no such lightmap. Four
    /// bytes a pixel, the last of them 255.
    ///
    /// A vertex that lies at `u`, `v` of the page lies in band `k` at
    ///
    ///     u, (v * side + k * changing_rows) / (layer_count * changing_rows)
    ///
    /// and what is shown of the face is the sum over its lightmaps of the
    /// pixel there times what `LightmapAtlasBlock::styles[k]` is worth, as
    /// `Compose` sums them. Empty for a page that is not one of them.
    [[nodiscard]] std::vector<std::uint8_t> ComposeLayers(std::uint32_t page) const;

    /// Makes the pixels anew for what the styles are worth now, and returns
    /// the pages that have to be handed to the renderer again, from the
    /// lowest. Only faces with a style whose value is another than at the
    /// last time are made anew, and only their own pixels and border are
    /// written, so that calling this with the same values costs next to
    /// nothing and returns no page, and that what a host painted elsewhere
    /// on a page stays.
    ///
    /// A pixel is the sum of the samples of its face, each multiplied by
    /// the value of its style, and 255 when the sum is more. A value counts
    /// in steps of a 256th, and below 0 as 0. A style of `LightStyles::count`
    /// or above, which no light of the game has, is always worth 1.
    std::vector<std::uint32_t> Compose(const LightStyles::Values &new_values);
  };
} // quake

#endif //QUAKE_LIGHTMAP_ATLAS_HPP
