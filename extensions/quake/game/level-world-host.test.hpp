#ifndef QUAKE_LEVEL_WORLD_HOST_TEST_HPP
#define QUAKE_LEVEL_WORLD_HOST_TEST_HPP

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>

#include "formats/bsp-file.hpp"
#include "formats/qc-machine.hpp"
#include "level-collision.hpp"
#include "level-vector.hpp"
#include "qc-builtin-number.hpp"
#include "qc-fields.hpp"

namespace quake
{
  /// The builtins that are a host's own, for the tests that run a level of
  /// a real game with the real builtins: an entity is placed, sized, and
  /// given a model as the fields of the game code have it, and what is
  /// only seen or heard does nothing.
  class LevelWorldHost final
  {
    using Number = QcBuiltinNumber;

    const BspFile &_level;
    LevelCollision &_collision;
    QcFields _fields;

    void SetSize(QcMachine &machine, const std::int32_t entity, const LevelVector &mins, const LevelVector &maxs) const
    {
      _fields.mins.Set(machine, entity, mins);
      _fields.maxs.Set(machine, entity, maxs);
      _fields.size.Set(machine, entity, Difference(maxs, mins));
      _collision.Link(entity);
    }

    void SetModel(QcMachine &machine) const
    {
      const std::int32_t entity = machine.GetParameterInteger(0);
      const std::string_view name = machine.GetParameterString(1);
      _fields.model.Set(machine, entity, machine.GetParameterInteger(1));

      // A part of the level, `*1`, `*2`, and so on, gives the entity its
      // size. Any other model leaves it without one: the game code sizes
      // what it gives such a model itself.
      std::size_t model = 0;
      const char *end = name.data() + name.size();
      if (name.starts_with('*') && std::from_chars(name.data() + 1, end, model).ptr == end &&
          model < _level.models.size())
      {
        const BspModel &part = _level.models[model];
        _fields.modelindex.Set(machine, entity, static_cast<float>(model + 1));
        SetSize(machine, entity, {part.mins.x, part.mins.y, part.mins.z}, {part.maxs.x, part.maxs.y, part.maxs.z});
        return;
      }
      SetSize(machine, entity, {}, {});
    }

  public:
    /// The level and the collision have to outlive this, and this the game
    /// code that runs on the machine.
    LevelWorldHost(const BspFile &level, LevelCollision &collision)
      : _level(level), _collision(collision), _fields(collision.GetMachine().GetProgs())
    {
    }

    LevelWorldHost(const LevelWorldHost &) = delete;

    LevelWorldHost &operator=(const LevelWorldHost &) = delete;

    void Register(QcMachine &machine)
    {
      const auto set = [&machine](const Number number, QcMachine::Builtin builtin)
      {
        machine.SetBuiltin(static_cast<std::int32_t>(number), std::move(builtin));
      };

      set(Number::SetOrigin, [this](QcMachine &m)
      {
        _fields.origin.Set(m, m.GetParameterInteger(0), m.GetParameterVector(1));
        _collision.Link(m.GetParameterInteger(0));
      });
      set(Number::SetSize, [this](QcMachine &m)
      {
        SetSize(m, m.GetParameterInteger(0), m.GetParameterVector(1), m.GetParameterVector(2));
      });
      set(Number::SetModel, [this](QcMachine &m) { SetModel(m); });

      for (const Number number : {Number::MakeStatic, Number::Sound, Number::AmbientSound, Number::Particle})
      {
        set(number, [](QcMachine &) {});
      }
    }
  };
} // quake

#endif //QUAKE_LEVEL_WORLD_HOST_TEST_HPP
