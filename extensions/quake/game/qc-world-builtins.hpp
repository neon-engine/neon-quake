#ifndef QUAKE_QC_WORLD_BUILTINS_HPP
#define QUAKE_QC_WORLD_BUILTINS_HPP

#include <cstdint>

#include "formats/qc-machine.hpp"
#include "level-collision.hpp"
#include "level-trace-result.hpp"
#include "qc-fields.hpp"
#include "qc-globals.hpp"

namespace quake
{
  /// The builtins of the original's engine that ask the level and the
  /// entities in it: what a line hits, what fills a place, where the floor
  /// is under an entity, who is near a place, and which player a monster
  /// may look for. `QcCoreBuiltins` has those that need no world.
  ///
  /// They are answered by a `LevelCollision`, in the units and axes of the
  /// game. What a host has the means for alone is not here: `setorigin`,
  /// `setsize`, `setmodel`, `makestatic`, `sound`, `ambientsound`, and
  /// `particle`.
  ///
  /// It serves one machine, the one of its collision. Register() puts the
  /// builtins on it, and they then call back into this object, which
  /// therefore stays where it is and lives for as long as the machine runs
  /// game code.
  class QcWorldBuiltins final
  {
    LevelCollision &_collision;
    QcFields _fields;
    QcGlobals _globals;
    std::int32_t _client_count = 1;

    /// Writes what a move met into the globals `trace_*` of the game code.
    void SetTraceGlobals(QcMachine &machine, const LevelTraceResult &trace) const;

    // The builtins that are more than a line. Each is called by the machine
    // with the parameters of the game code in place.

    void TraceLine(QcMachine &machine) const;

    void DropToFloor(QcMachine &machine) const;

    void FindRadius(QcMachine &machine) const;

    void CheckClient(QcMachine &machine) const;

  public:
    /// How far down `droptofloor` looks for a floor.
    static constexpr float drop_distance = 256.0f;

    /// Takes the collision that answers, which has to outlive this.
    explicit QcWorldBuiltins(LevelCollision &collision);

    // the builtins on the machine point back here
    QcWorldBuiltins(const QcWorldBuiltins &) = delete;

    QcWorldBuiltins &operator=(const QcWorldBuiltins &) = delete;

    /// Registers the builtins on a machine, in the place of what it had for
    /// their numbers:
    ///
    /// - `traceline(v1, v2, nomonsters, ent)` moves a point and leaves what
    ///   it met in the globals `trace_*`. `nomonsters` is a number of
    ///   `LevelTraceKind`, and `ent` is passed, see LevelCollision::Trace().
    ///   `trace_ent` is the world when nothing was hit.
    /// - `pointcontents(v)` gives what fills a place of the world, a number
    ///   of `BspContents`.
    /// - `droptofloor()` puts `self` on what is under it, the level or an
    ///   entity, up to `drop_distance` down, and gives 1. It gives 0 and
    ///   leaves `self` where it is when nothing is that near, or when it
    ///   is inside what is solid.
    /// - `findradius(v, radius)` gives the entities whose middle is within
    ///   a radius of a place, as a list through the field `chain`.
    /// - `checkclient()` gives a player a monster may look for, see
    ///   SetClientCount().
    /// - `aim(ent, speed)` gives the global `v_forward`: where the player
    ///   looks. The original bends the shot towards what is nearly in
    ///   line, for players who cannot look up and down. That is left out.
    void Register(QcMachine &machine);

    /// How many players there may be: the entities 1 to `count`, which the
    /// original keeps for them. One at the start.
    ///
    /// `checkclient` gives one of them who is alive, `health` above 0, and
    /// does not have the flag `NoTarget`, another every tenth of a second
    /// when there are several, and the world when there is none. The
    /// original also asks whether the player may be seen from where the
    /// monster stands, by the visibility the level carries. That is left
    /// out here: the game code goes on to ask how far the player is and
    /// whether a line reaches them, so a monster still wakes for a player
    /// it sees, only with more asking.
    void SetClientCount(std::int32_t count);

    [[nodiscard]] std::int32_t GetClientCount() const;
  };
} // quake

#endif //QUAKE_QC_WORLD_BUILTINS_HPP
