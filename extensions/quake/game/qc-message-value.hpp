#ifndef QUAKE_QC_MESSAGE_VALUE_HPP
#define QUAKE_QC_MESSAGE_VALUE_HPP

#include <cstdint>
#include <string_view>

#include "qc-message-kind.hpp"

namespace quake
{
  /// One part of a message of the network, as the game code wrote it: the
  /// kind, and the value in the member of that kind. The others are zero or
  /// empty.
  struct QcMessageValue
  {
    QcMessageKind kind = QcMessageKind::Byte;

    /// The value of every kind that is a number, as the game code has it.
    float number = 0.0f;

    /// The value of a String. It is a string of the machine, and stays for
    /// as long as the machine lives.
    std::string_view text;

    /// The value of an Entity, its number.
    std::int32_t entity = 0;
  };
} // quake

#endif //QUAKE_QC_MESSAGE_VALUE_HPP
