#ifndef QUAKE_LIGHTMAP_ATLAS_HPP
#define QUAKE_LIGHTMAP_ATLAS_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "bsp-mesh.hpp"
#include "lightmap-atlas-block.hpp"
#include "lightmap-atlas-page.hpp"
#include "lightmap-atlas-vertex.hpp"

namespace quake
{
  /// The lightmaps of all faces of a mesh of a level in one picture, or in a
  /// few, and where every vertex of the mesh lies there. A renderer draws a
  /// level with a handful of pictures of light then, not with one for each
  /// of its thousands of faces.
  ///
  /// It is made from a `BspMesh` and keeps nothing of it.
  ///
  /// **Which light.** Only the first lightmap of a face is taken, the one of
  /// its first style, which is the light that never changes in nearly every
  /// face. The further ones, of lights that flicker or are switched, are
  /// left out for now, so such a light is missing from the atlas.
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

    /// Packs the lightmaps of a mesh. `largest_side` is how many pixels a
    /// page may be wide and high at most. Returns false, says why in
    /// `error`, and stays as it was when `largest_side` is smaller than
    /// `smallest_side`, when a face names vertices the mesh does not have,
    /// when the lightmap of a face has not the bytes its size and styles ask
    /// for, or when a face with its border is larger than a page.
    bool Build(const BspMesh &mesh, std::string &error, std::uint32_t largest_side = default_largest_side);
  };
} // quake

#endif //QUAKE_LIGHTMAP_ATLAS_HPP
