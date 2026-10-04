#ifndef QUAKE_LEVEL_TRACE_KIND_HPP
#define QUAKE_LEVEL_TRACE_KIND_HPP

namespace quake
{
  /// What a move through a level is stopped by, with the numbers the game
  /// code hands `traceline` for it.
  enum class LevelTraceKind
  {
    /// The level, and every entity that stops things.
    Normal = 0,
    /// The level and what is a part of it, a door or a lift. No box of an
    /// entity stops the move.
    NoMonsters = 1,
    /// As `Normal`, but a monster is hit in a box 15 units larger to every
    /// side than the moving box, so that a missile does not fly past it
    /// by a hair.
    Missile = 2,
  };
} // quake

#endif //QUAKE_LEVEL_TRACE_KIND_HPP
