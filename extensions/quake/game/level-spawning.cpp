#include "level-spawning.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <optional>
#include <string>

#include "formats/progs-definition.hpp"
#include "formats/qc-cell.hpp"
#include "qc-move-type.hpp"
#include "qc-solid.hpp"

namespace quake
{
  // Helpers of LevelSpawning: values of the text of a level, taken apart.
  namespace
  {
    /// A text of a level as the game code is to see it: the two characters
    /// `\n` are one new line.
    std::string WithNewLines(const std::string_view value)
    {
      std::string text;
      text.reserve(value.size());
      for (std::size_t i = 0; i < value.size(); i++)
      {
        if (value[i] == '\\' && i + 1 < value.size() && value[i + 1] == 'n')
        {
          text += '\n';
          i++;
          continue;
        }
        text += value[i];
      }
      return text;
    }

    /// Three numbers with spaces between. What is missing or is no number
    /// is zero.
    std::array<float, 3> ParseVector(const std::string &value)
    {
      std::array<float, 3> vector{};
      const char *at = value.c_str();
      for (float &part : vector)
      {
        char *end = nullptr;
        part = std::strtof(at, &end);
        if (end == at) { break; }
        at = end;
      }
      return vector;
    }
  }

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

    const std::span<QcCell> cells = _machine.GetEntity(entity);
    const std::size_t offset = definition->offset;
    const ProgsType type = definition->GetType();
    if (offset + (type == ProgsType::Vector ? 3u : 1u) > cells.size()) { return false; }

    switch (type)
    {
      case ProgsType::String:
      {
        // the machine may move the entities when it grows, not when it
        // keeps a string
        cells[offset] = QcCell::OfInteger(_machine.AddString(WithNewLines(text)));
        return true;
      }
      case ProgsType::Float:
      {
        cells[offset] = QcCell::OfFloat(std::strtof(text.c_str(), nullptr));
        return true;
      }
      case ProgsType::Vector:
      {
        const std::array<float, 3> vector = ParseVector(text);
        for (std::size_t i = 0; i < vector.size(); i++) { cells[offset + i] = QcCell::OfFloat(vector[i]); }
        return true;
      }
      case ProgsType::Entity:
      {
        cells[offset] = QcCell::OfInteger(std::atoi(text.c_str()));
        return true;
      }
      case ProgsType::Field:
      {
        const ProgsDefinition *named = progs.FindField(text);
        if (named == nullptr) { return false; }
        cells[offset] = QcCell::OfInteger(named->offset);
        return true;
      }
      case ProgsType::Function:
      {
        const std::optional<std::int32_t> function = progs.FindFunction(text);
        if (!function) { return false; }
        cells[offset] = QcCell::OfInteger(*function);
        return true;
      }
      default:
      {
        return false;
      }
    }
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
