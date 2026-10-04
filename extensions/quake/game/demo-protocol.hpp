#ifndef QUAKE_DEMO_PROTOCOL_HPP
#define QUAKE_DEMO_PROTOCOL_HPP

#include <cstdint>

namespace quake
{
  /// Which protocol the messages of a server are written in. A server says
  /// it first of all, and how every later message is read depends on it.
  struct DemoProtocol
  {
    /// The protocol of the original.
    static constexpr std::int32_t netquake = 15;

    /// That of FitzQuake: more models, sounds, and entities, and entities
    /// that are seen through.
    static constexpr std::int32_t fitzquake = 666;

    /// That of RMQ: as FitzQuake, with flags that say how a coordinate and
    /// an angle are written.
    static constexpr std::int32_t rmq = 999;

    // The flags of RMQ.

    /// An angle is a short, a 65536th of a turn.
    static constexpr std::uint32_t short_angle = 1u << 1;
    /// An angle is a float, in degrees.
    static constexpr std::uint32_t float_angle = 1u << 2;
    /// A coordinate is a short for the whole units and a byte for the 255ths.
    static constexpr std::uint32_t coord_24_bit = 1u << 3;
    /// A coordinate is a float.
    static constexpr std::uint32_t float_coord = 1u << 4;
    /// An entity may have a scale.
    static constexpr std::uint32_t edict_scale = 1u << 5;
    /// A coordinate is a long, in 16ths of a unit.
    static constexpr std::uint32_t int32_coord = 1u << 7;

    std::int32_t version = netquake;

    /// The flags, which only RMQ has. Zero reads as the original: a
    /// coordinate is a short in 8ths of a unit, an angle a char, a 256th
    /// of a turn.
    std::uint32_t flags = 0;

    /// Whether a version is one that can be read.
    [[nodiscard]] static constexpr bool IsKnown(const std::int32_t version)
    {
      return version == netquake || version == fitzquake || version == rmq;
    }

    /// Whether the messages have what FitzQuake added.
    [[nodiscard]] constexpr bool HasExtensions() const
    {
      return version == fitzquake || version == rmq;
    }
  };
} // quake

#endif //QUAKE_DEMO_PROTOCOL_HPP
