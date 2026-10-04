#ifndef QUAKE_QC_GLOBALS_HPP
#define QUAKE_QC_GLOBALS_HPP

#include <array>
#include <string_view>
#include <vector>

#include "formats/progs-definition.hpp"
#include "formats/progs.hpp"
#include "qc-global.hpp"

namespace quake
{
  /// The globals of the game code that the engine of the original works
  /// with, each found by its name in a program: who is thinking, what time
  /// it is, what a trace hit, and the functions the engine calls.
  ///
  /// The original had them as a struct laid over the first globals, and
  /// refused a program whose layout was another. Here they are looked up,
  /// so a program that has them elsewhere works, and one that lacks some
  /// still runs: such a global reads as zero and is not written.
  ///
  /// ```
  /// const QcGlobals globals(machine.GetProgs());
  /// globals.time.Set(machine, 1.0f);
  /// const std::int32_t thinker = globals.self.Get(machine);
  /// ```
  struct QcGlobals
  {
    using Float = QcGlobal<ProgsType::Float>;
    using Vector = QcGlobal<ProgsType::Vector>;
    using String = QcGlobal<ProgsType::String>;
    using Entity = QcGlobal<ProgsType::Entity>;
    using Function = QcGlobal<ProgsType::Function>;

    /// How many numbers a player carries from one level to the next.
    static constexpr std::size_t parm_count = 16;

    // Who a function runs for, and whom it meets.
    Entity self;
    Entity other;
    Entity world;

    // The time of the level in seconds, and how long a frame is.
    Float time;
    Float frametime;

    // The level, and what is counted through it.
    String mapname;
    Float serverflags;
    Float total_secrets;
    Float total_monsters;
    Float found_secrets;
    Float killed_monsters;

    /// `parm1` to `parm16`, the first at 0.
    std::array<Float, parm_count> parms;

    // The three axes `makevectors` leaves.
    Vector v_forward;
    Vector v_up;
    Vector v_right;

    // What `traceline` leaves.
    Float trace_allsolid;
    Float trace_startsolid;
    Float trace_fraction;
    Vector trace_endpos;
    Vector trace_plane_normal;
    Float trace_plane_dist;
    Entity trace_ent;
    Float trace_inopen;
    Float trace_inwater;

    /// To whom a message of the game code goes.
    Entity msg_entity;

    /// For how many frames more everything is to touch its triggers anew.
    Float force_retouch;

    // The kind of game.
    Float deathmatch;
    Float coop;
    Float teamplay;

    // The functions of the game code the engine calls.
    Function main;
    Function StartFrame;
    Function PlayerPreThink;
    Function PlayerPostThink;
    Function ClientKill;
    Function ClientConnect;
    Function PutClientInServer;
    Function ClientDisconnect;
    Function SetNewParms;
    Function SetChangeParms;

    /// The names of those the program does not have, or has with another
    /// type.
    std::vector<std::string_view> missing;

    /// Looks every one up in a program.
    explicit QcGlobals(const Progs &progs);

    /// Whether those are found without which no level runs: `self`,
    /// `other`, `world`, `time`, and `frametime`.
    [[nodiscard]] bool HasEssentials() const;
  };
} // quake

#endif //QUAKE_QC_GLOBALS_HPP
