#include "demo-file.hpp"

#include <format>

#include "formats/byte-reader.hpp"

namespace quake
{
  // The limits of the first line, which need nothing of the object.
  namespace
  {
    /// The most digits the number of a track may have. A track is a byte.
    constexpr std::size_t max_track_digits = 9;
  }

  bool DemoFile::Read(const std::span<const std::uint8_t> bytes, std::string &problem)
  {
    _forced_track = no_forced_track;
    _blocks.clear();
    _is_cut_short = false;

    // the line of the track: a number that may be negative, and a newline
    std::size_t position = 0;
    const bool is_negative = position < bytes.size() && bytes[position] == '-';
    if (is_negative) { position++; }

    std::int32_t track = 0;
    std::size_t digit_count = 0;
    while (position < bytes.size() && bytes[position] >= '0' && bytes[position] <= '9')
    {
      if (++digit_count > max_track_digits)
      {
        problem = "The number of the music track at the start is too long";
        return false;
      }
      track = track * 10 + (bytes[position] - '0');
      position++;
    }
    if (digit_count == 0 || position >= bytes.size() || bytes[position] != '\n')
    {
      problem = "The file does not start with a line that has the number of a music track";
      return false;
    }
    position++;
    _forced_track = is_negative ? -track : track;

    ByteReader reader(bytes);
    reader.Seek(position);
    while (reader.GetPosition() < reader.GetSize())
    {
      const std::size_t start = reader.GetPosition();
      const std::int32_t size = reader.ReadI32();
      DemoBlock block;
      for (float &angle : block.view_angles) { angle = reader.ReadF32(); }
      if (!reader.IsGood())
      {
        _is_cut_short = true;
        break;
      }
      if (size < 0 || static_cast<std::size_t>(size) > max_message_size)
      {
        problem = std::format(
          "Block {} at byte {} says it has {} bytes, and a block has at most {}",
          _blocks.size(), start, size, max_message_size);
        _blocks.clear();
        return false;
      }

      block.message = reader.ReadBytes(static_cast<std::size_t>(size));
      if (!reader.IsGood())
      {
        _is_cut_short = true;
        break;
      }
      _blocks.push_back(block);
    }
    return true;
  }

  std::int32_t DemoFile::GetForcedTrack() const
  {
    return _forced_track;
  }

  std::span<const DemoBlock> DemoFile::GetBlocks() const
  {
    return _blocks;
  }

  bool DemoFile::IsCutShort() const
  {
    return _is_cut_short;
  }
} // quake
