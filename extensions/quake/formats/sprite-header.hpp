#ifndef QUAKE_SPRITE_HEADER_HPP
#define QUAKE_SPRITE_HEADER_HPP

#include <cstdint>

#include "sprite-orientation.hpp"

namespace quake
{
  /// What a sprite says of itself at its start, as the file has it.
  struct SpriteHeader
  {
    SpriteOrientation orientation = SpriteOrientation::Parallel;

    /// How far from its origin the sprite reaches at most.
    float bounding_radius = 0.0f;

    /// The width and height of its largest pictures, in pixels.
    std::int32_t width = 0;
    std::int32_t height = 0;

    std::int32_t frame_count = 0;

    /// A length the original game never did anything with.
    float beam_length = 0.0f;

    /// How the groups of frames keep time: 0 for in step with every other
    /// sprite of its kind, 1 for each starting at a time of its own.
    std::int32_t sync_type = 0;
  };
} // quake

#endif //QUAKE_SPRITE_HEADER_HPP
