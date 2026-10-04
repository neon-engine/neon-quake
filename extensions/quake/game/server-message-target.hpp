#ifndef QUAKE_SERVER_MESSAGE_TARGET_HPP
#define QUAKE_SERVER_MESSAGE_TARGET_HPP

#include <cstdint>

#include "qc-message-destination.hpp"

namespace quake
{
  /// For whom the game code wrote a message.
  struct ServerMessageTarget
  {
    QcMessageDestination destination = QcMessageDestination::Broadcast;

    /// The entity of the one player the message is for when the destination
    /// is One, and 0, the world, otherwise.
    std::int32_t client = 0;
  };
} // quake

#endif //QUAKE_SERVER_MESSAGE_TARGET_HPP
