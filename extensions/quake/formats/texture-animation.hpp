#ifndef QUAKE_TEXTURE_ANIMATION_HPP
#define QUAKE_TEXTURE_ANIMATION_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "mip-texture.hpp"

namespace quake
{
  /// The pictures a texture of a level changes through: a screen that
  /// flickers, an eye that looks around, a button that is lit until it is
  /// pressed.
  ///
  /// A level says so with the names of its textures. Those that start with
  /// `+0` to `+9` and go on with the same letters are shown in turn, each
  /// for a fifth of a second. Those that start with `+a` to `+j` are a
  /// second run of the same name, which a model of the level shows in place
  /// of the first while the game code has set its frame to something else
  /// than 0: a button that was pressed.
  struct TextureAnimation
  {
    /// How many pictures are shown in a second.
    static constexpr double frames_per_second = 5.0;

    /// The textures that are shown in turn, as their places in the list of
    /// the level, in their order.
    std::vector<std::int32_t> frames;

    /// The textures that are shown in their place while the frame of the
    /// model is not 0. Empty when the name has no second run.
    std::vector<std::int32_t> alternate;

    /// The runs a texture of a list belongs to. A texture of the second run
    /// has the second run as its frames and the first as its alternate, as
    /// the original has it. A texture that is not part of any, and a place
    /// that is not in the list, has the texture alone as its frames.
    [[nodiscard]] static TextureAnimation Find(
      std::span<const std::optional<MipTexture>> textures,
      std::int32_t texture);

    /// Which of `count` pictures is shown at a time, in seconds. 0 for no
    /// pictures.
    [[nodiscard]] static std::size_t ChooseFrame(std::size_t count, double time);

    /// Whether anything ever changes: more than one picture in a run, or a
    /// second run.
    [[nodiscard]] bool Changes() const
    {
      return frames.size() > 1 || !alternate.empty();
    }
  };
} // quake

#endif //QUAKE_TEXTURE_ANIMATION_HPP
