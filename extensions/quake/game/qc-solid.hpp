#ifndef QUAKE_QC_SOLID_HPP
#define QUAKE_QC_SOLID_HPP

namespace quake
{
  /// What an entity is to what runs into it, the number the game code writes
  /// in the field `solid`.
  enum class QcSolid
  {
    /// Nothing touches it.
    Not = 0,
    /// It is touched and stops nothing.
    Trigger = 1,
    /// Its box is touched and stops what runs into it.
    BoundingBox = 2,
    /// The same, for what moves by itself: a player, a monster.
    SlideBox = 3,
    /// It stops with the shape of its model, a part of the level.
    Bsp = 4,
  };
} // quake

#endif //QUAKE_QC_SOLID_HPP
