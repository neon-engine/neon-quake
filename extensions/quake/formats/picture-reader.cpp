#include "picture-reader.hpp"

#include <array>
#include <cstddef>
#include <utility>

#include "byte-reader.hpp"

namespace quake
{
  // Helpers of the readers of pictures: the files that are no pictures, and
  // the checks every picture goes through.
  namespace
  {
    /// How many bytes the width and the height take in front of the pixels.
    constexpr std::size_t header_size = 8;

    /// The `.lmp` files that are not pictures, each with what it is.
    constexpr std::array<std::pair<std::string_view, std::string_view>, 3> not_pictures = {{
      {"gfx/palette.lmp", "the palette, 768 bytes of colours"},
      {"gfx/colormap.lmp", "the table that darkens the colours of the palette"},
      {"gfx/pop.lmp", "the mark of the registered game"},
    }};

    /// Whether two paths are the same, upper and lower case being one.
    bool same_path(const std::string_view one, const std::string_view other)
    {
      if (one.size() != other.size()) { return false; }

      const auto lower = [](const char letter)
      {
        return letter >= 'A' && letter <= 'Z' ? static_cast<char>(letter - 'A' + 'a') : letter;
      };
      for (std::size_t i = 0; i < one.size(); i++)
      {
        if (lower(one[i]) != lower(other[i])) { return false; }
      }
      return true;
    }

    /// Whether a width and a height are ones a picture can have, saying in
    /// `error` when not.
    bool check_size(const std::int32_t width, const std::int32_t height, std::string &error)
    {
      if (width < 1 || height < 1 || width > Picture::max_side || height > Picture::max_side)
      {
        error = "a picture cannot be " + std::to_string(width) + " by " + std::to_string(height) +
          " pixels: a side is from 1 to " + std::to_string(Picture::max_side);
        return false;
      }
      return true;
    }

    /// Takes the pixels that are left in a reader as a picture of a size
    /// that was checked already, refusing any other number of them.
    bool take_pixels(
      ByteReader &reader,
      const std::int32_t width,
      const std::int32_t height,
      Picture &picture,
      std::string &error)
    {
      // neither side is more than `max_side`, so this cannot wrap around
      const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
      const std::size_t left = reader.GetSize() - reader.GetPosition();
      if (left != count)
      {
        error = "a picture of " + std::to_string(width) + " by " + std::to_string(height) + " pixels has " +
          std::to_string(count) + " bytes of them, and there are " + std::to_string(left);
        return false;
      }

      const auto pixels = reader.ReadBytes(count);
      picture.width = width;
      picture.height = height;
      picture.pixels.assign(pixels.begin(), pixels.end());
      return true;
    }
  }

  bool read_picture(const std::span<const std::uint8_t> bytes, Picture &picture, std::string &error)
  {
    ByteReader reader(bytes);
    const std::int32_t width = reader.ReadI32();
    const std::int32_t height = reader.ReadI32();
    if (!reader.IsGood())
    {
      error = "a picture starts with " + std::to_string(header_size) +
        " bytes for its width and height, and there are " + std::to_string(bytes.size());
      return false;
    }

    if (!check_size(width, height, error)) { return false; }
    return take_pixels(reader, width, height, picture, error);
  }

  bool read_raw_picture(
    const std::span<const std::uint8_t> bytes,
    const std::int32_t width,
    const std::int32_t height,
    Picture &picture,
    std::string &error)
  {
    if (!check_size(width, height, error)) { return false; }

    ByteReader reader(bytes);
    return take_pixels(reader, width, height, picture, error);
  }

  bool read_picture_file(
    const std::string_view path,
    const std::span<const std::uint8_t> bytes,
    Picture &picture,
    std::string &error)
  {
    for (const auto &[not_picture, what_it_is] : not_pictures)
    {
      if (same_path(path, not_picture))
      {
        error = std::string(not_picture) + " is not a picture: it is " + std::string(what_it_is);
        return false;
      }
    }
    return read_picture(bytes, picture, error);
  }
} // quake
