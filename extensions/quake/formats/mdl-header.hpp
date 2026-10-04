#ifndef QUAKE_MDL_HEADER_HPP
#define QUAKE_MDL_HEADER_HPP

#include <cstdint>

#include "mdl-vector.hpp"

namespace quake
{
  /// What a model says of itself at its start, as the file has it.
  struct MdlHeader
  {
    /// What the three bytes of a vertex are multiplied with, and what is
    /// added after, to give its place in the units of the game.
    MdlVector scale;
    MdlVector translate;

    /// How far from its origin the model reaches at most.
    float bounding_radius = 0.0f;

    /// Where the eyes of the model are, which is from where it sees.
    MdlVector eye_position;

    std::int32_t skin_count = 0;

    /// The size of every skin, in pixels.
    std::int32_t skin_width = 0;
    std::int32_t skin_height = 0;

    std::int32_t vertex_count = 0;
    std::int32_t triangle_count = 0;
    std::int32_t frame_count = 0;

    /// How the groups of frames keep time: 0 for in step with every other
    /// model of its kind, 1 for each starting at a time of its own.
    std::int32_t sync_type = 0;

    /// The effects the game gives the model, a bit for each: the trail of a
    /// rocket, the spinning of an item.
    std::uint32_t flags = 0;

    /// The average size of a triangle on the screen, which the original
    /// renderer chose its detail with.
    float size = 0.0f;
  };
} // quake

#endif //QUAKE_MDL_HEADER_HPP
