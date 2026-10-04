#include "sprite-file.hpp"

#include <utility>

#include "byte-reader.hpp"

namespace quake
{
  // Helpers of SpriteFile: one reader per part of the file, and the checks
  // they share.
  namespace
  {
    /// How many bytes the file has of each thing.
    constexpr std::size_t header_size = 36;
    constexpr std::size_t picture_header_size = 16;

    /// Whether `count` things of `size` bytes each are left to read. Asked
    /// before a list is made, so that a count that lies is refused without
    /// making room for it.
    bool has_room(const ByteReader &reader, const std::size_t count, const std::size_t size)
    {
      const std::size_t left = reader.GetSize() - reader.GetPosition();
      return size == 0 || count <= left / size;
    }

    /// Whether a count of the file is between 1 and the most there may be,
    /// saying so in `error` when not.
    bool check_count(const std::int32_t count, const std::int32_t most, const std::string &what, std::string &error)
    {
      if (count >= 1 && count <= most) { return true; }

      error = "The " + what + ", " + std::to_string(count) + ", is not between 1 and " + std::to_string(most);
      return false;
    }

    bool read_header(ByteReader &reader, SpriteHeader &header, std::string &error)
    {
      if (!has_room(reader, 1, header_size))
      {
        error = "The file ends before its header";
        return false;
      }

      if (reader.ReadU32() != SpriteFile::magic)
      {
        error = "The file does not start with IDSP";
        return false;
      }
      if (const std::int32_t version = reader.ReadI32(); version != SpriteFile::version)
      {
        error = "The version is " + std::to_string(version) + ", not " + std::to_string(SpriteFile::version);
        return false;
      }

      const std::int32_t orientation = reader.ReadI32();
      if (orientation < 0 || orientation > static_cast<std::int32_t>(SpriteOrientation::ParallelOriented))
      {
        error = "The way the sprite turns, " + std::to_string(orientation) + ", is not between 0 and 4";
        return false;
      }
      header.orientation = static_cast<SpriteOrientation>(orientation);

      header.bounding_radius = reader.ReadF32();
      header.width = reader.ReadI32();
      header.height = reader.ReadI32();
      header.frame_count = reader.ReadI32();
      header.beam_length = reader.ReadF32();
      header.sync_type = reader.ReadI32();

      return check_count(header.width, SpriteFile::most_picture_side, "width of the sprite", error) &&
        check_count(header.height, SpriteFile::most_picture_side, "height of the sprite", error) &&
        check_count(header.frame_count, SpriteFile::most_frames, "number of frames", error);
    }

    /// A picture: where it hangs, its size, and its pixels. `which` names
    /// it in what is said of a mistake.
    bool read_picture(ByteReader &reader, const std::string &which, SpritePicture &picture, std::string &error)
    {
      if (!has_room(reader, 1, picture_header_size))
      {
        error = "The file ends before the end of " + which;
        return false;
      }
      picture.left = reader.ReadI32();
      picture.up = reader.ReadI32();
      picture.width = reader.ReadI32();
      picture.height = reader.ReadI32();

      if (!check_count(picture.width, SpriteFile::most_picture_side, "width of " + which, error) ||
        !check_count(picture.height, SpriteFile::most_picture_side, "height of " + which, error))
      {
        return false;
      }

      const std::size_t size = static_cast<std::size_t>(picture.width) * static_cast<std::size_t>(picture.height);
      if (!has_room(reader, 1, size))
      {
        error = "The file ends before the end of " + which;
        return false;
      }
      const auto pixels = reader.ReadBytes(size);
      picture.pixels.assign(pixels.begin(), pixels.end());
      return true;
    }

    bool read_frame(ByteReader &reader, const std::size_t number, SpriteFrame &frame, std::string &error)
    {
      const std::string which = "frame " + std::to_string(number);
      const std::string cut_short = "The file ends before the end of " + which;

      if (!has_room(reader, 1, 4))
      {
        error = cut_short;
        return false;
      }
      frame.is_group = reader.ReadI32() != 0;

      if (!frame.is_group)
      {
        SpritePicture picture;
        if (!read_picture(reader, which, picture, error)) { return false; }

        frame.pictures.push_back(std::move(picture));
        return true;
      }

      if (!has_room(reader, 1, 4))
      {
        error = cut_short;
        return false;
      }
      const std::int32_t count = reader.ReadI32();
      if (!check_count(count, SpriteFile::most_in_group, "number of pictures of " + which, error)) { return false; }

      // The times, and after them the pictures, each of which has at least
      // where it hangs and its size: a count that lies is refused before
      // room is made for what is not there.
      const auto picture_count = static_cast<std::size_t>(count);
      if (!has_room(reader, picture_count, 4 + picture_header_size))
      {
        error = cut_short;
        return false;
      }
      frame.times.reserve(picture_count);
      for (std::size_t i = 0; i < picture_count; i++)
      {
        frame.times.push_back(reader.ReadF32());
      }

      frame.pictures.reserve(picture_count);
      for (std::size_t i = 0; i < picture_count; i++)
      {
        SpritePicture picture;
        if (!read_picture(reader, "picture " + std::to_string(i) + " of " + which, picture, error))
        {
          return false;
        }
        frame.pictures.push_back(std::move(picture));
      }
      return true;
    }
  }

  bool SpriteFile::Read(const std::span<const std::uint8_t> bytes, std::string &error)
  {
    ByteReader reader(bytes);

    SpriteHeader header;
    if (!read_header(reader, header, error)) { return false; }

    std::vector<SpriteFrame> frames;
    for (std::size_t i = 0; i < static_cast<std::size_t>(header.frame_count); i++)
    {
      SpriteFrame frame;
      if (!read_frame(reader, i, frame, error)) { return false; }
      frames.push_back(std::move(frame));
    }

    // every read was asked for before it was made, so this cannot be, and
    // is asked all the same
    if (!reader.IsGood())
    {
      error = "The file ends before what it announced";
      return false;
    }

    _header = header;
    _frames = std::move(frames);
    return true;
  }

  const SpriteHeader &SpriteFile::GetHeader() const
  {
    return _header;
  }

  const std::vector<SpriteFrame> &SpriteFile::GetFrames() const
  {
    return _frames;
  }

  const SpritePicture *SpriteFile::FindPicture(const std::size_t frame, const std::size_t picture) const
  {
    if (frame >= _frames.size() || picture >= _frames[frame].pictures.size()) { return nullptr; }
    return &_frames[frame].pictures[picture];
  }
} // quake
