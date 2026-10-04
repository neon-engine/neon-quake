#ifndef QUAKE_PICTURE_READER_HPP
#define QUAKE_PICTURE_READER_HPP

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "picture.hpp"

namespace quake
{
  /// Reads a picture from the bytes of a `.lmp` file, or of a lump of
  /// `gfx.wad` that holds a picture of the status bar: the width and the
  /// height, four bytes each, and then a byte for every pixel.
  ///
  /// Returns false, says in `error` what was wrong, and leaves `picture` as
  /// it was, when the bytes are not that: too few for the two numbers, a
  /// side that is less than one or more than `Picture::max_side`, or a
  /// number of bytes after them that is not the width times the height.
  bool read_picture(std::span<const std::uint8_t> bytes, Picture &picture, std::string &error);

  /// Reads a picture that has no width and height in front of its pixels,
  /// from the size the caller knows it to have. The letters of the console,
  /// `conchars` of `gfx.wad`, are such a picture, of 128 by 128.
  ///
  /// Returns false, says in `error` what was wrong, and leaves `picture` as
  /// it was, when a side is less than one or more than `Picture::max_side`,
  /// or the bytes are not exactly the width times the height.
  bool read_raw_picture(
    std::span<const std::uint8_t> bytes,
    std::int32_t width,
    std::int32_t height,
    Picture &picture,
    std::string &error);

  /// Reads a picture from the bytes of a file, given the path it has in the
  /// game's data, such as `gfx/qplaque.lmp`.
  ///
  /// Not every `.lmp` file is a picture, and the path is the only thing that
  /// tells: `gfx/palette.lmp` is the palette, which `Palette` reads,
  /// `gfx/colormap.lmp` is the table that darkens colours, and `gfx/pop.lmp`
  /// is the mark of the registered game. These are refused, with an `error`
  /// that says what the file is instead. Upper and lower case in the path
  /// are the same. Any other path is read as read_picture() does.
  bool read_picture_file(
    std::string_view path,
    std::span<const std::uint8_t> bytes,
    Picture &picture,
    std::string &error);
} // quake

#endif //QUAKE_PICTURE_READER_HPP
