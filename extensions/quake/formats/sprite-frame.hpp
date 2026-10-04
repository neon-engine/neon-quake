#ifndef QUAKE_SPRITE_FRAME_HPP
#define QUAKE_SPRITE_FRAME_HPP

#include <vector>

#include "sprite-picture.hpp"

namespace quake
{
  /// One frame of a sprite, which is what the game code names by a number:
  /// a picture, or a group of pictures that plays by itself.
  struct SpriteFrame
  {
    /// Whether the file has this frame as a group, which may hold one
    /// picture only.
    bool is_group = false;

    /// One picture, or the pictures of the group in their order.
    std::vector<SpritePicture> pictures;

    /// For a group, the time in seconds since the start of the group at
    /// which each picture ends, one for each picture. Empty otherwise.
    std::vector<float> times;
  };
} // quake

#endif //QUAKE_SPRITE_FRAME_HPP
