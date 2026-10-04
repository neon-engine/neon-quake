#ifndef QUAKE_SPRITE_FILE_HPP
#define QUAKE_SPRITE_FILE_HPP

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "sprite-frame.hpp"
#include "sprite-header.hpp"
#include "sprite-picture.hpp"

namespace quake
{
  /// A sprite of the game, `progs/*.spr`: a flat picture that stands in the
  /// world and turns to whoever looks at it, as an explosion or a bubble
  /// does. A header, and frames of pictures that may each have a size of
  /// their own.
  ///
  /// What was read can be trusted: there are as many frames as the header
  /// says, every frame has a picture, and every picture has as many pixels
  /// as its width and height make.
  class SpriteFile
  {
    SpriteHeader _header;
    std::vector<SpriteFrame> _frames;

  public:
    /// The four letters a sprite starts with, `IDSP`, read as a number.
    static constexpr std::uint32_t magic = 0x50534449;

    /// The only version the original game has of the format.
    static constexpr std::int32_t version = 1;

    /// The most a sprite may have of each thing, past what any has, which
    /// only keeps a file that lies from asking for more memory than there
    /// is.
    static constexpr std::int32_t most_frames = 65536;
    static constexpr std::int32_t most_in_group = 65536;
    static constexpr std::int32_t most_picture_side = 8192;

    /// Takes the sprite from the bytes of the file. Returns false, says in
    /// `error` what was wrong, and stays as it was, when the bytes are not a
    /// sprite: another magic or version, a way of turning there is not, a
    /// count or a size that is negative or past the most there may be, a
    /// file that ends before what it announced. Bytes after the last frame
    /// are left alone.
    bool Read(std::span<const std::uint8_t> bytes, std::string &error);

    [[nodiscard]] const SpriteHeader &GetHeader() const;

    [[nodiscard]] const std::vector<SpriteFrame> &GetFrames() const;

    /// A picture of a frame: `picture` is 0 for a frame that is not a
    /// group, and the place in the group otherwise. Null when there is no
    /// such frame or no such picture in it.
    [[nodiscard]] const SpritePicture *FindPicture(std::size_t frame, std::size_t picture = 0) const;
  };
} // quake

#endif //QUAKE_SPRITE_FILE_HPP
