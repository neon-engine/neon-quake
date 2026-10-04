#ifndef QUAKE_DEMO_ENTITY_BIT_HPP
#define QUAKE_DEMO_ENTITY_BIT_HPP

#include <cstdint>

namespace quake
{
  /// The bits of the update of an entity: each says that a value follows,
  /// or, for a few, something of the update itself. The lowest eight are
  /// the first byte of the message, whose highest bit marks it as an
  /// update.
  enum class DemoEntityBit : std::uint32_t
  {
    /// A second byte of bits follows.
    MoreBits = 1u << 0,
    Origin1 = 1u << 1,
    Origin2 = 1u << 2,
    Origin3 = 1u << 3,
    Angle2 = 1u << 4,
    /// The entity moves in steps, as monsters do: nothing follows.
    Step = 1u << 5,
    Frame = 1u << 6,
    /// The message is an update.
    Signal = 1u << 7,
    Angle1 = 1u << 8,
    Angle3 = 1u << 9,
    Model = 1u << 10,
    Colormap = 1u << 11,
    Skin = 1u << 12,
    Effects = 1u << 13,
    /// The number of the entity is a short, not a byte.
    LongEntity = 1u << 14,

    // What FitzQuake added.

    /// A third byte of bits follows.
    Extend1 = 1u << 15,
    Alpha = 1u << 16,
    /// The high byte of the frame.
    Frame2 = 1u << 17,
    /// The high byte of the model.
    Model2 = 1u << 18,
    /// When the next frame of the model is due.
    LerpFinish = 1u << 19,
    /// The scale, of RMQ.
    Scale = 1u << 20,
    /// A fourth byte of bits follows.
    Extend2 = 1u << 23,
  };
} // quake

#endif //QUAKE_DEMO_ENTITY_BIT_HPP
