#ifndef QUAKE_LEVEL_RUNNING_HPP
#define QUAKE_LEVEL_RUNNING_HPP

#include <cstddef>
#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "client-think.hpp"
#include "formats/qc-machine.hpp"
#include "level-caller.hpp"
#include "level-failure.hpp"
#include "level-mover.hpp"
#include "level-spawning.hpp"
#include "qc-fields.hpp"
#include "qc-globals.hpp"

namespace quake
{
  /// Lets time pass for a level that `LevelSpawning` started, in steps of a
  /// fixed length, as the engine of the original does in every frame.
  ///
  /// The game code does nothing by itself. An entity says when it wants to
  /// think next, in its field `nextthink`, and with which function, in
  /// `think`; this calls them when their time has come. What pushes, a door
  /// or a lift, has a clock of its own, which runs while it waits to think
  /// and nothing blocks it, and is moved here by its velocity for as long
  /// as that clock runs. Everything else is moved by a `LevelMover`, since
  /// moving takes the walls of the level.
  ///
  /// A function that is stopped does not end the frame. It is kept as a
  /// `LevelFailure` for the host to read.
  class LevelRunning final : public LevelCaller
  {
    QcMachine &_machine;
    QcGlobals _globals;
    QcFields _fields;
    LevelMover *_mover = nullptr;

    /// How many of the first entities after the world are players that
    /// have their whole frame here.
    std::int32_t _client_count = 0;

    /// The time of the level, which the global `time` is set from.
    double _time = LevelSpawning::start_time;

    std::vector<LevelFailure> _failures;
    std::size_t _failure_count = 0;

    /// Calls a function at a time of the level. False, with the failure
    /// kept, when the run was stopped.
    bool Run(std::int32_t function, std::int32_t self, std::int32_t other, float time);

    /// Calls a function the engine knows by a global, such as `StartFrame`.
    /// False, with nothing kept, when the program has no such function.
    bool RunNamed(const QcGlobals::Function &place, std::string_view name, std::int32_t self);

    /// Moves what pushes by its velocity for a time of its own clock, when
    /// the mover lets it.
    void Push(std::int32_t entity, float dt);

    /// A frame of an entity that pushes: it moves, up to the time it wants
    /// to think at, and thinks when its clock has reached that.
    void AdvancePusher(std::int32_t entity, float dt);

    /// A frame of any other entity: it thinks when its time has come. False
    /// when the thought removed it.
    bool AdvanceThinker(std::int32_t entity, float dt);

    /// A frame of a player: what the game code does before a player
    /// moves, the player's own thought, the move, and what the game code
    /// does after.
    void AdvanceClient(std::int32_t entity, float dt);

  public:
    /// How many failures are kept. The first ones are, and the rest are
    /// only counted: a think that fails does so in every frame.
    static constexpr std::size_t max_failures_kept = 64;

    /// The machine, and the mover when there is one, must outlive this.
    explicit LevelRunning(QcMachine &machine, LevelMover *mover = nullptr);

    /// Gives the level its mover after it was made, for a mover that calls
    /// the game code through this and so is made after it. Null for none.
    void SetMover(LevelMover *mover);

    /// The time of the level in seconds. A level starts at
    /// `LevelSpawning::start_time`.
    [[nodiscard]] double GetTime() const;

    /// For a level that goes on from a saved game.
    void SetTime(double time);

    /// How many players have their whole frame in Advance(): the entities 1
    /// to this, which the original keeps for its players. For each of them
    /// that is not free, Advance() runs `PlayerPreThink`, lets the player
    /// think, has the mover move the player, and runs `PlayerPostThink`,
    /// all at the player's turn among the entities, as the original does.
    ///
    /// None at the start: a host then moves the player itself, and calls
    /// RunClientThink() around that. A host that sets this does not call
    /// RunClientThink() any more, and counts only players that are in the
    /// level, see ConnectClient().
    void SetClientCount(std::int32_t client_count);

    [[nodiscard]] std::int32_t GetClientCount() const;

    /// Lets `dt` seconds pass.
    ///
    /// The game code starts the frame with `StartFrame`. Then every entity
    /// that is not free has its turn, in the order of their numbers, those
    /// made during the frame as well. Last the time moves on.
    ///
    /// The original runs two frames of a tenth of a second right after a
    /// level started, before any player is in, for what falls to land. A
    /// host does the same with two calls.
    void Advance(float dt);

    /// Lets a player into the level, as the entity kept for one: the game
    /// code gives the numbers a new player starts with, `SetNewParms`,
    /// greets the player, `ClientConnect`, and puts the player where the
    /// level starts one, `PutClientInServer`. A name that is not empty is
    /// the player's `netname`. False when one of the three was stopped or
    /// is not in the program.
    ///
    /// A player who comes from another level brings the numbers that were
    /// taken there with SaveClient(), `parms`, which are then put in the
    /// place of those of a new player: what the player carries and how well
    /// the player is. Anything but as many as the game has is taken as none.
    bool ConnectClient(std::int32_t entity, std::string_view name = {}, std::span<const float> parms = {});

    /// The numbers a player takes along to the next level, as the game code
    /// works them out with `SetChangeParms`. Those of a new player, zeros,
    /// when the program has no such function or it was stopped.
    [[nodiscard]] std::array<float, QcGlobals::parm_count> SaveClient(std::int32_t entity);

    /// Runs what the game code does for a player before or after the player
    /// is moved in a frame. A host calls it around its own moving of the
    /// player, once for each frame.
    bool RunClientThink(std::int32_t entity, ClientThink moment);

    /// Calls a function of the game code for an entity, with the time of
    /// the level, as this does for a think: for a host that has an entity
    /// touch another, or be blocked by one. False, with the failure kept,
    /// when the run was stopped. Nothing is run for function 0, which is no
    /// function.
    bool RunFunction(std::int32_t function, std::int32_t self, std::int32_t other) override;

    /// The first failures since the last ClearFailures(), and how many
    /// there were in all.
    [[nodiscard]] const std::vector<LevelFailure> &GetFailures() const;

    [[nodiscard]] std::size_t GetFailureCount() const;

    void ClearFailures();
  };
} // quake

#endif //QUAKE_LEVEL_RUNNING_HPP
