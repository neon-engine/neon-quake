#ifndef QUAKE_BSP_LIGHT_FACE_HPP
#define QUAKE_BSP_LIGHT_FACE_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "bsp-face.hpp"
#include "bsp-vector.hpp"

namespace quake
{
  /// A face of a level as `BspLightPoint` keeps it: where a place lies on
  /// its texture, how far its lightmap reaches there, and where the samples
  /// of the lightmap are.
  ///
  /// A place lies at `s = place . s_axis + s_offset` and likewise `t` on the
  /// texture. The lightmap has a sample every sixteen units of both, and its
  /// first sample lies at `first_s` and `first_t`.
  struct BspLightFace
  {
    /// Whether it holds light back: every face does but the sky and the
    /// liquids, which are looked through. A face no fork of the world names
    /// is left as one that does not.
    bool is_met = false;

    /// Whether it has samples. A face that is met and has none is one no
    /// light of the level reaches: the tools that light a level write no
    /// lightmap for it, and it is dark.
    bool is_lit = false;

    BspVector s_axis;
    float s_offset = 0.0f;
    BspVector t_axis;
    float t_offset = 0.0f;

    /// Where the first sample lies on the texture: the last line of sixteen
    /// at or before the least corner of the face.
    double first_s = 0.0;
    double first_t = 0.0;

    /// How many samples the lightmap is wide and high, or would be if the
    /// face had one.
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    /// What lights each of its lightmaps, and how many there are.
    std::array<std::uint8_t, 4> styles{
      BspFace::no_style, BspFace::no_style, BspFace::no_style, BspFace::no_style};
    std::uint32_t style_count = 0;

    /// The number of its first sample among all samples of the level.
    std::size_t first_sample = 0;
  };
} // quake

#endif //QUAKE_BSP_LIGHT_FACE_HPP
