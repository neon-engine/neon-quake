#include "level-spawning.hpp"

#include <algorithm>
#include <optional>
#include <string>

#include "formats/progs-definition.hpp"
#include "formats/qc-cell.hpp"
#include "qc-move-type.hpp"
#include "qc-solid.hpp"
#include "qc-value-text.hpp"

namespace quake
{
  LevelSpawning::LevelSpawning(QcMachine &machine)
    : _machine(machine), _globals(machine.GetProgs()), _fields(machine.GetProgs())
  {
  }

  bool LevelSpawning::SetFieldFromText(const std::int32_t entity, std::string_view key, const std::string_view value)
  {
    // some levels have a key with spaces after it
    while (key.ends_with(' ')) { key.remove_suffix(1); }

    std::string text(value);

    // one number for where a thing looks is the yaw of its angles
    if (key == "angle")
    {
      key = "angles";
      text = "0 " + text + " 0";
    }
    // the brightness of a light is not the field `light`, which is the
    // function that makes one
    if (key == "light") { key = "light_lev"; }

    const Progs &progs = _machine.GetProgs();
    const ProgsDefinition *definition = progs.FindField(key);
    if (definition == nullptr) { return false; }

    const std::size_t offset = definition->offset;
    const ProgsType type = definition->GetType();
    const std::size_t size = QcValueText::GetSize(type);
    if (offset + size > _machine.GetEntity(entity).size()) { return false; }

    QcValueText::Cells value_cells;
    if (!QcValueText::Parse(_machine, type, text, value_cells)) { return false; }

    // asked for after the value was made: the machine keeps a string then
    const std::span<QcCell> cells = _machine.GetEntity(entity);
    for (std::size_t i = 0; i < size; i++) { cells[offset + i] = value_cells[i]; }
    return true;
  }

  bool LevelSpawning::IsLeftOut(const std::int32_t entity, const LevelSpawningSettings &settings)
  {
    const auto flags = static_cast<std::int32_t>(_fields.spawnflags.Get(_machine, entity));

    // a deathmatch has no skill
    if (settings.deathmatch != 0.0f) { return (flags & not_in_deathmatch) != 0; }

    const int skill = std::clamp(settings.skill, 0, 3);
    if (skill == 0) { return (flags & not_on_easy) != 0; }
    if (skill == 1) { return (flags & not_on_medium) != 0; }
    return (flags & not_on_hard) != 0;
  }

  LevelSpawningReport LevelSpawning::Spawn(
    const std::span<const BspEntity> entities, const LevelSpawningSettings &settings)
  {
    LevelSpawningReport report;

    _globals.mapname.SetText(_machine, settings.map_name);
    _globals.time.Set(_machine, start_time);
    _globals.serverflags.Set(_machine, settings.server_flags);
    _globals.deathmatch.Set(_machine, settings.deathmatch);
    _globals.coop.Set(_machine, settings.coop);

    // The world is the first model of the level. It never moves, and is
    // still what pushes: the game code and the engine treat it as they
    // treat a door.
    _fields.model.SetText(_machine, 0, settings.model_name);
    _fields.modelindex.Set(_machine, 0, 1.0f);
    _fields.solid.Set(_machine, 0, static_cast<float>(QcSolid::Bsp));
    _fields.movetype.Set(_machine, 0, static_cast<float>(QcMoveType::Push));

    // The players come first, so that the game code and the host find them
    // by their numbers, 1 for the first.
    for (std::int32_t player = 0; player < settings.player_count; player++)
    {
      if (!_machine.CreateEntity()) { break; }
    }

    for (std::size_t i = 0; i < entities.size(); i++)
    {
      std::int32_t entity = 0;
      if (i > 0)
      {
        const std::optional<std::int32_t> made = _machine.CreateEntity();
        if (!made)
        {
          report.without_room = entities.size() - i;
          break;
        }
        entity = *made;
      }

      for (const auto &[key, value] : entities[i].pairs)
      {
        // a key for the compiler of levels, which no game code reads
        if (key.starts_with('_')) { continue; }

        if (!SetFieldFromText(entity, key, value)) { report.unknown_keys++; }
      }

      // the world is in every game
      if (entity != 0 && IsLeftOut(entity, settings))
      {
        _machine.FreeEntity(entity);
        report.left_out++;
        continue;
      }

      const std::string classname(_fields.classname.GetText(_machine, entity));
      const std::optional<std::int32_t> function =
        classname.empty() ? std::nullopt : _machine.GetProgs().FindFunction(classname);
      if (!function)
      {
        _machine.FreeEntity(entity);
        report.without_function++;
        if (std::ranges::find(report.classnames_without_function, classname) ==
            report.classnames_without_function.end())
        {
          report.classnames_without_function.push_back(classname);
        }
        continue;
      }

      _globals.self.Set(_machine, entity);
      if (_machine.Call(*function))
      {
        report.spawned++;
        continue;
      }

      report.failed++;
      report.failures.push_back({
        .entity = entity, .classname = classname, .function = classname, .error = _machine.GetError(),
      });
    }
    return report;
  }
} // quake
