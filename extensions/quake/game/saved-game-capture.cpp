#include "saved-game-capture.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "formats/progs-definition.hpp"
#include "formats/progs.hpp"
#include "formats/qc-cell.hpp"
#include "qc-value-text.hpp"

namespace quake
{
  // Helpers of SavedGameCapture: what is saved of a program, and the values
  // of a saved game on their way into a machine.
  namespace
  {
    /// Whether a saved game keeps a global of this type.
    bool IsSavedGlobal(const ProgsType type)
    {
      return type == ProgsType::String || type == ProgsType::Float || type == ProgsType::Entity;
    }

    /// Whether a saved game keeps a field of this type and name.
    bool IsSavedField(const ProgsType type, const std::string_view name)
    {
      if (type == ProgsType::Void || type > ProgsType::Function) { return false; }

      // the parts of a vector, `origin_x`, are saved with the vector
      return name.size() < 2 || name[name.size() - 2] != '_';
    }

    /// A value that was made from its text, and where it goes: an offset
    /// among the globals, or among the cells of an entity.
    struct Write
    {
      std::int32_t entity = 0;
      std::size_t offset = 0;
      std::size_t size = 0;
      QcValueText::Cells cells;
    };

    /// Makes the cells of a value of a saved game. `where` names it for
    /// the error.
    bool MakeValue(
      QcMachine &machine, const ProgsDefinition &definition, const std::string &text, const std::size_t entity_count,
      const std::string &where, Write &write, std::string &error)
    {
      const ProgsType type = definition.GetType();
      write.offset = definition.offset;
      write.size = QcValueText::GetSize(type);
      if (!QcValueText::Parse(machine, type, text, write.cells))
      {
        error = std::format("{} of the saved game is `{}`, which the program does not have", where, text);
        return false;
      }

      // a number of no entity would only stop the game code later
      if (type == ProgsType::Entity)
      {
        const std::int32_t entity = write.cells[0].AsInteger();
        if (entity < 0 || static_cast<std::size_t>(entity) >= entity_count)
        {
          error = std::format(
            "{} of the saved game is entity {}, and the saved game has {} entities", where, entity, entity_count);
          return false;
        }
      }
      return true;
    }
  }

  SavedGame SavedGameCapture::Capture(QcMachine &machine, SavedGame game)
  {
    const Progs &progs = machine.GetProgs();

    game.globals.clear();
    for (const ProgsDefinition &definition : progs.global_definitions)
    {
      const ProgsType type = definition.GetType();
      if (!definition.IsSaved() || !IsSavedGlobal(type)) { continue; }

      const std::array cells = {QcCell::OfInteger(machine.GetInteger(definition.offset))};
      game.globals.emplace_back(progs.GetString(definition.name), QcValueText::Print(machine, type, cells));
    }

    game.entities.clear();
    game.entities.reserve(static_cast<std::size_t>(machine.GetEntityCount()));
    for (std::int32_t entity = 0; entity < machine.GetEntityCount(); entity++)
    {
      SavedGameEntity &saved = game.entities.emplace_back();
      if (machine.IsEntityFree(entity))
      {
        saved.free = true;
        continue;
      }

      const std::span<const QcCell> cells = machine.GetEntity(entity);
      for (const ProgsDefinition &definition : progs.field_definitions)
      {
        const ProgsType type = definition.GetType();
        const std::string_view name = progs.GetString(definition.name);
        const std::size_t size = QcValueText::GetSize(type);
        if (!IsSavedField(type, name) || definition.offset + size > cells.size()) { continue; }

        const std::span<const QcCell> value = cells.subspan(definition.offset, size);
        if (std::ranges::all_of(value, [](const QcCell &cell) { return cell.bits == 0; })) { continue; }

        saved.pairs.emplace_back(name, QcValueText::Print(machine, type, value));
      }
    }
    return game;
  }

  bool SavedGameCapture::Restore(
    const SavedGame &game, QcMachine &machine, std::string &error, SavedGameRestoreReport *report)
  {
    const Progs &progs = machine.GetProgs();
    const std::size_t entity_count = game.entities.size();
    if (entity_count == 0)
    {
      error = "The saved game has no entity, not even the world";
      return false;
    }

    // Every value is made first, so that a saved game that is refused
    // leaves the machine as it was, but for the strings it keeps.
    SavedGameRestoreReport counts;
    std::vector<Write> global_writes;
    for (const auto &[name, text] : game.globals)
    {
      const ProgsDefinition *definition = progs.FindGlobal(name);
      if (definition == nullptr)
      {
        counts.unknown_globals++;
        continue;
      }

      Write write;
      if (!MakeValue(machine, *definition, text, entity_count, std::format("The global `{}`", name), write, error))
      {
        return false;
      }
      if (write.offset + write.size > static_cast<std::size_t>(machine.GetGlobalCount()))
      {
        error = std::format("The global `{}` of the saved game is outside the globals of the program", name);
        return false;
      }
      global_writes.push_back(write);
    }

    // the first of a name is the field, as a search through them finds it
    std::unordered_map<std::string_view, const ProgsDefinition *> fields;
    for (const ProgsDefinition &definition : progs.field_definitions)
    {
      fields.emplace(progs.GetString(definition.name), &definition);
    }
    const auto entity_fields = static_cast<std::size_t>(progs.header.entity_fields);

    std::vector<Write> field_writes;
    for (std::size_t entity = 0; entity < entity_count; entity++)
    {
      if (game.entities[entity].free) { continue; }

      for (const auto &[name, text] : game.entities[entity].pairs)
      {
        const auto found = fields.find(name);
        if (found == fields.end())
        {
          counts.unknown_fields++;
          continue;
        }

        Write write;
        write.entity = static_cast<std::int32_t>(entity);
        if (!MakeValue(
          machine, *found->second, text, entity_count, std::format("The field `{}` of entity {}", name, entity),
          write, error))
        {
          return false;
        }
        if (write.offset + write.size > entity_fields)
        {
          error = std::format("The field `{}` of the saved game is outside the cells of an entity", name);
          return false;
        }
        field_writes.push_back(write);
      }
    }

    // CreateEntity() fills the place of a free entity before it makes a new
    // one, so every free one is taken first, and all are there in the end.
    std::size_t free_count = 0;
    for (std::int32_t entity = 0; entity < machine.GetEntityCount(); entity++)
    {
      if (machine.IsEntityFree(entity)) { free_count++; }
    }
    for (std::size_t i = 0; i < free_count; i++) { machine.CreateEntity(); }
    while (static_cast<std::size_t>(machine.GetEntityCount()) < entity_count)
    {
      if (!machine.CreateEntity())
      {
        error = std::format(
          "The saved game has {} entities, and the machine has room for {}", entity_count, machine.GetEntityCount());
        return false;
      }
    }

    for (std::int32_t entity = 0; entity < machine.GetEntityCount(); entity++)
    {
      std::ranges::fill(machine.GetEntity(entity), QcCell{});
    }
    for (const Write &write : field_writes)
    {
      const std::span<QcCell> cells = machine.GetEntity(write.entity);
      for (std::size_t i = 0; i < write.size; i++) { cells[write.offset + i] = write.cells[i]; }
    }
    for (const Write &write : global_writes)
    {
      for (std::size_t i = 0; i < write.size; i++)
      {
        machine.SetInteger(static_cast<std::int32_t>(write.offset + i), write.cells[i].AsInteger());
      }
    }

    // the world is never free
    for (std::int32_t entity = 1; entity < machine.GetEntityCount(); entity++)
    {
      const auto index = static_cast<std::size_t>(entity);
      if (index >= entity_count || game.entities[index].free || game.entities[index].pairs.empty())
      {
        machine.FreeEntity(entity);
      }
    }

    if (report != nullptr) { *report = counts; }
    return true;
  }
} // quake
