#ifndef QUAKE_QC_CORE_BUILTINS_HPP
#define QUAKE_QC_CORE_BUILTINS_HPP

#include <array>
#include <cstdint>
#include <initializer_list>
#include <random>
#include <string>
#include <string_view>

#include "formats/qc-cell.hpp"
#include "formats/qc-machine.hpp"
#include "qc-console-variables.hpp"
#include "qc-host.hpp"
#include "qc-message-kind.hpp"
#include "qc-precache-list.hpp"

namespace quake
{
  /// The builtins of the original's engine that need no world: no level to
  /// collide with, nothing to draw, nothing to hear. The arithmetic, the
  /// text, the entities of the machine, the console variables, the files the
  /// game code names, the styles of the lights, and what is only handed on
  /// to the host.
  ///
  /// What a builtin leaves behind is kept here for the host to read: the
  /// console variables, the lists of models and sounds, the styles of the
  /// lights. What the host has to act on, a line of text, an entity that
  /// went, a change of level, reaches it through `QcHost`.
  ///
  /// It serves one machine. Register() puts the builtins on it, and they
  /// then call back into this object, which therefore stays where it is and
  /// lives for as long as the machine runs game code. The builtins that need
  /// a world, `setorigin`, `traceline`, `sound` and the like, are not
  /// registered here.
  class QcCoreBuiltins final
  {
  public:
    /// How many styles of lights there are, numbered from 0.
    static constexpr std::int32_t light_style_count = 64;

    /// What the error of a run starts with that `error` stopped, for the
    /// host to tell it from a fault of the machine or of the program.
    static constexpr std::string_view error_start = "The game code says: ";

  private:
    QcHost &_host;
    QcConsoleVariables _variables;
    QcPrecacheList _models;
    QcPrecacheList _sounds;
    std::array<std::string, light_style_count> _light_styles;

    /// What `random` takes its numbers from. The same seed gives the same
    /// numbers on every machine.
    std::mt19937 _random;

    /// Where the globals and fields are that the builtins work on, found by
    /// their names once, in Register(). -1 for one the program does not
    /// have: such a global reads as zero and is not written, such a field
    /// is left out.
    struct Places
    {
      std::int32_t self = -1;
      std::int32_t message_entity = -1;
      std::int32_t forward = -1;
      std::int32_t right = -1;
      std::int32_t up = -1;

      // the fields the original resets when an entity is removed
      std::int32_t model = -1;
      std::int32_t take_damage = -1;
      std::int32_t model_index = -1;
      std::int32_t color_map = -1;
      std::int32_t skin = -1;
      std::int32_t frame = -1;
      std::int32_t solid = -1;
      std::int32_t origin = -1;
      std::int32_t angles = -1;
      std::int32_t next_think = -1;
    };

    Places _places;

    void FindPlaces(const Progs &progs);

    /// Sets cells of an entity from a field on, where the entity has them.
    static void SetCells(QcMachine &machine, std::int32_t entity, std::int32_t field,
      std::initializer_list<QcCell> cells);

    // The builtins that are more than a line. Each is called by the machine
    // with the parameters of the game code in place.

    void MakeVectors(QcMachine &machine) const;

    void Find(QcMachine &machine) const;

    void NextEntity(QcMachine &machine) const;

    void Spawn(QcMachine &machine);

    void RaiseError(QcMachine &machine);

    void RaiseObjectError(QcMachine &machine);

    void SetLightStyle(QcMachine &machine);

    void Write(QcMachine &machine, QcMessageKind kind);

  public:
    /// Takes the host the builtins hand on to, which has to outlive this.
    explicit QcCoreBuiltins(QcHost &host);

    // the builtins on the machine point back here
    QcCoreBuiltins(const QcCoreBuiltins &) = delete;

    QcCoreBuiltins &operator=(const QcCoreBuiltins &) = delete;

    /// Registers the builtins on a machine, in the place of what it had for
    /// their numbers, and finds the globals and fields of its program that
    /// they work on.
    void Register(QcMachine &machine);

    /// The console variables: read by `cvar`, written by `cvar_set`, and
    /// set by the host to what a player chose.
    [[nodiscard]] QcConsoleVariables &GetVariables();

    [[nodiscard]] const QcConsoleVariables &GetVariables() const;

    /// The models the game code named with `precache_model`. The host adds
    /// the level itself first, as number 1.
    [[nodiscard]] QcPrecacheList &GetModels();

    [[nodiscard]] const QcPrecacheList &GetModels() const;

    /// The sounds the game code named with `precache_sound`.
    [[nodiscard]] QcPrecacheList &GetSounds();

    [[nodiscard]] const QcPrecacheList &GetSounds() const;

    /// How the lights of a style flicker, as `lightstyle` set it: a letter
    /// for each tenth of a second. Empty for a style that was never set and
    /// for one there is not.
    [[nodiscard]] std::string_view GetLightStyle(std::int32_t style) const;

    /// Starts the numbers of `random` anew from a seed: with the time for a
    /// game, with a fixed number for a test or a recording.
    void SeedRandom(std::uint32_t seed);

    /// The next number of `random`: more than 0 and less than 1, never
    /// either, since game code multiplies it and takes the whole part for
    /// one of a few choices.
    float NextRandom();

    /// Removes an entity as the engine of the original does, for `remove`
    /// and for a host that takes one away itself: the machine frees it, the
    /// fields that say what it shows and when it thinks are reset, and the
    /// host is told with QcHost::EntityRemoved(). False, with nothing done,
    /// for the world, a free entity, and one there is not.
    bool RemoveEntity(QcMachine &machine, std::int32_t entity);
  };
} // quake

#endif //QUAKE_QC_CORE_BUILTINS_HPP
