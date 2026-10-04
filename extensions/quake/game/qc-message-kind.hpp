#ifndef QUAKE_QC_MESSAGE_KIND_HPP
#define QUAKE_QC_MESSAGE_KIND_HPP

namespace quake
{
  /// What a part of a message of the network is, one for each of the
  /// builtins `WriteByte` to `WriteEntity`.
  enum class QcMessageKind
  {
    Byte,
    Char,
    Short,
    Long,
    /// A place along one axis, in units of the game.
    Coord,
    /// An angle in degrees.
    Angle,
    String,
    Entity,
  };
} // quake

#endif //QUAKE_QC_MESSAGE_KIND_HPP
