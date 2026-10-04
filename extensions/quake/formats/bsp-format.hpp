#ifndef QUAKE_BSP_FORMAT_HPP
#define QUAKE_BSP_FORMAT_HPP

namespace quake
{
  /// The forms a level is written in. They have the same lumps and differ
  /// in how wide the numbers of some of them are, which is what limits how
  /// large a level can be.
  enum class BspFormat
  {
    /// The one of the original game, whose file starts with the number 29.
    /// Most of what it counts is kept in 16 bits.
    Version29,

    /// The one of the tools that lift the limits, whose file starts with
    /// the letters `BSP2`: what version 29 keeps in 16 bits is kept in 32,
    /// and the boxes of nodes and leaves are numbers with a fraction.
    Bsp2,

    /// An older try at the same, whose file starts with the letters `2PSB`:
    /// as `Bsp2`, with the boxes of nodes and leaves still in 16 bits.
    Bsp2Rmq,
  };
} // quake

#endif //QUAKE_BSP_FORMAT_HPP
