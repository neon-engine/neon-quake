#ifndef QUAKE_MDL_SKIN_HPP
#define QUAKE_MDL_SKIN_HPP

#include <cstdint>
#include <vector>

namespace quake
{
  /// One skin of a model: a picture, or a group of pictures shown one after
  /// the other.
  ///
  /// A picture is one byte for each pixel, the number of a colour of the
  /// palette, row after row from the top, of the width and height the header
  /// of the model gives. `Palette::ToRgba` makes four bytes of each.
  struct MdlSkin
  {
    /// Whether the file has this skin as a group, which may hold one picture
    /// only.
    bool is_group = false;

    /// One picture, or the pictures of the group in their order.
    std::vector<std::vector<std::uint8_t>> pictures;

    /// For a group, the time in seconds since the start of the group at
    /// which each picture ends, one for each picture. Empty otherwise.
    std::vector<float> times;
  };
} // quake

#endif //QUAKE_MDL_SKIN_HPP
