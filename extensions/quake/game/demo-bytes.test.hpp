#ifndef QUAKE_DEMO_BYTES_TEST_HPP
#define QUAKE_DEMO_BYTES_TEST_HPP

#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>
#include <vector>

#include "level-vector.hpp"

namespace quake
{
  /// Writes bytes as a server and a recording have them, for the tests:
  /// the values of a message, lowest byte first, and the blocks of a file.
  ///
  /// ```
  /// DemoBytes message;
  /// message.Byte(7).Float(1.5f);
  /// DemoBytes file;
  /// file.Line("-1").Block({0, 90, 0}, message);
  /// ```
  class DemoBytes final
  {
  public:
    std::vector<std::uint8_t> bytes;

    DemoBytes &Byte(const std::uint32_t value)
    {
      bytes.push_back(static_cast<std::uint8_t>(value));
      return *this;
    }

    /// A byte with a sign.
    DemoBytes &Char(const std::int32_t value)
    {
      return Byte(static_cast<std::uint32_t>(value));
    }

    DemoBytes &Short(const std::int32_t value)
    {
      const auto bits = static_cast<std::uint32_t>(value);
      return Byte(bits).Byte(bits >> 8);
    }

    DemoBytes &Long(const std::int64_t value)
    {
      const auto bits = static_cast<std::uint32_t>(value);
      return Byte(bits).Byte(bits >> 8).Byte(bits >> 16).Byte(bits >> 24);
    }

    DemoBytes &Float(const float value)
    {
      std::uint32_t bits;
      std::memcpy(&bits, &value, sizeof(bits));
      return Long(bits);
    }

    /// A text with its zero.
    DemoBytes &Text(const std::string_view text)
    {
      for (const char letter : text) { Byte(static_cast<std::uint8_t>(letter)); }
      return Byte(0);
    }

    /// A coordinate as the original writes it: a short, in 8ths.
    DemoBytes &Coord(const float value)
    {
      return Short(static_cast<std::int32_t>(value * 8.0f));
    }

    /// Three coordinates as the original writes them.
    DemoBytes &Place(const LevelVector &place)
    {
      return Coord(place[0]).Coord(place[1]).Coord(place[2]);
    }

    /// An angle as the original writes it: a char, in 256ths of a turn.
    DemoBytes &Angle(const float degrees)
    {
      return Char(static_cast<std::int32_t>(degrees * 256.0f / 360.0f));
    }

    /// Other bytes as they are.
    DemoBytes &Add(const DemoBytes &other)
    {
      bytes.insert(bytes.end(), other.bytes.begin(), other.bytes.end());
      return *this;
    }

    /// A line of text with its newline, as a recording starts with.
    DemoBytes &Line(const std::string_view text)
    {
      for (const char letter : text) { Byte(static_cast<std::uint8_t>(letter)); }
      return Byte('\n');
    }

    /// A block of a recording: where the player looked, and a packet.
    DemoBytes &Block(const LevelVector &view_angles, const DemoBytes &message)
    {
      Long(static_cast<std::int64_t>(message.bytes.size()));
      Float(view_angles[0]).Float(view_angles[1]).Float(view_angles[2]);
      return Add(message);
    }

    [[nodiscard]] std::span<const std::uint8_t> Get() const
    {
      return bytes;
    }
  };
} // quake

#endif //QUAKE_DEMO_BYTES_TEST_HPP
