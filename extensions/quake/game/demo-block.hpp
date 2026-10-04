#ifndef QUAKE_DEMO_BLOCK_HPP
#define QUAKE_DEMO_BLOCK_HPP

#include <cstdint>
#include <span>

#include "level-vector.hpp"

namespace quake
{
  /// One block of a `DemoFile`: what the server sent the one who recorded
  /// in one packet, and where that player looked when it came.
  struct DemoBlock
  {
    /// Pitch, yaw, and roll in degrees. The server does not send them: the
    /// one who recorded turned the view on their own machine.
    LevelVector view_angles{};

    /// The messages of the packet, one after the other, for a
    /// `DemoMessageReader`. They are bytes of the file, not a copy.
    std::span<const std::uint8_t> message;
  };
} // quake

#endif //QUAKE_DEMO_BLOCK_HPP
