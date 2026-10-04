#ifndef QUAKE_QC_MOVE_TYPE_HPP
#define QUAKE_QC_MOVE_TYPE_HPP

namespace quake
{
  /// How an entity moves, the number the game code writes in the field
  /// `movetype`. The field is a float, so these are compared with it as
  /// numbers.
  enum class QcMoveType
  {
    /// It never moves.
    None = 0,
    AngleNoClip = 1,
    AngleClip = 2,
    /// A player on the ground: it walks, and climbs steps.
    Walk = 3,
    /// A monster: it falls, and moves only by the steps it takes itself.
    Step = 4,
    Fly = 5,
    /// It falls and stays where it lands.
    Toss = 6,
    /// A door or a lift: nothing stops it but what it cannot push away, and
    /// it has a clock of its own, the field `ltime`.
    Push = 7,
    /// It goes through everything.
    NoClip = 8,
    FlyMissile = 9,
    Bounce = 10,
  };
} // quake

#endif //QUAKE_QC_MOVE_TYPE_HPP
