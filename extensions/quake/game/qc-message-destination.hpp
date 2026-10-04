#ifndef QUAKE_QC_MESSAGE_DESTINATION_HPP
#define QUAKE_QC_MESSAGE_DESTINATION_HPP

#include <cstdint>

namespace quake
{
  /// To whom the game code writes a part of a message of the network, with
  /// the numbers it has for them.
  enum class QcMessageDestination : std::int32_t
  {
    /// To every player, and it may be lost on the way.
    Broadcast = 0,

    /// To one player, the entity in the global `msg_entity`, and it arrives.
    One = 1,

    /// To every player, and it arrives.
    All = 2,

    /// To every player who joins, now or later: what a level starts with.
    Init = 3,
  };
} // quake

#endif //QUAKE_QC_MESSAGE_DESTINATION_HPP
