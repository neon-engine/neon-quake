#ifndef QUAKE_QC_FIELD_HPP
#define QUAKE_QC_FIELD_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>

#include "formats/progs-definition.hpp"
#include "formats/progs.hpp"
#include "formats/qc-cell.hpp"
#include "formats/qc-machine.hpp"

namespace quake
{
  /// Where one field is in every entity of the game code: found once by its
  /// name, then read and written for an entity without a search.
  ///
  /// It is to the fields what `QcGlobal` is to the globals, with the same
  /// types. A field the program does not have, or has with another type,
  /// and an entity that there is not, read as zero and refuse a write.
  template <ProgsType type>
  class QcField final
  {
    /// The offset among the cells of an entity, or -1.
    std::int32_t _offset = -1;

    /// How many cells the field is.
    static constexpr std::size_t size = type == ProgsType::Vector ? 3 : 1;

    /// The cells of the field in an entity. Empty when there are none.
    [[nodiscard]] std::span<QcCell> FindCells(QcMachine &machine, const std::int32_t entity) const
    {
      const std::span<QcCell> cells = machine.GetEntity(entity);
      if (_offset < 0 || static_cast<std::size_t>(_offset) + size > cells.size()) { return {}; }

      return cells.subspan(static_cast<std::size_t>(_offset), size);
    }

  public:
    /// What Get() gives and Set() takes.
    using Value = std::conditional_t<
      type == ProgsType::Float, float,
      std::conditional_t<type == ProgsType::Vector, std::array<float, 3>, std::int32_t>>;

    /// A place that is not found.
    QcField() = default;

    QcField(const Progs &progs, const std::string_view name)
    {
      const ProgsDefinition *definition = progs.FindField(name);
      if (definition != nullptr && definition->GetType() == type) { _offset = definition->offset; }
    }

    /// Whether the program has a field of the name and the type.
    [[nodiscard]] bool IsFound() const
    {
      return _offset >= 0;
    }

    /// The offset among the cells of an entity, or -1. It is what the game
    /// code hands a builtin that takes a field.
    [[nodiscard]] std::int32_t GetOffset() const
    {
      return _offset;
    }

    /// The machine is not const here, since it hands the cells of an entity
    /// out to be written as well.
    [[nodiscard]] Value Get(QcMachine &machine, const std::int32_t entity) const
    {
      const std::span<QcCell> cells = FindCells(machine, entity);
      if (cells.empty()) { return {}; }

      if constexpr (type == ProgsType::Float) { return cells[0].AsFloat(); }
      else if constexpr (type == ProgsType::Vector)
      {
        return {cells[0].AsFloat(), cells[1].AsFloat(), cells[2].AsFloat()};
      }
      else { return cells[0].AsInteger(); }
    }

    bool Set(QcMachine &machine, const std::int32_t entity, const Value &value) const
    {
      const std::span<QcCell> cells = FindCells(machine, entity);
      if (cells.empty()) { return false; }

      if constexpr (type == ProgsType::Float) { cells[0] = QcCell::OfFloat(value); }
      else if constexpr (type == ProgsType::Vector)
      {
        for (std::size_t i = 0; i < size; i++) { cells[i] = QcCell::OfFloat(value[i]); }
      }
      else { cells[0] = QcCell::OfInteger(value); }
      return true;
    }

    /// The text of a field that is a string. Empty when there is none.
    [[nodiscard]] std::string_view GetText(QcMachine &machine, const std::int32_t entity) const
      requires (type == ProgsType::String)
    {
      return machine.GetString(Get(machine, entity));
    }

    /// Sets a field that is a string to a text, which the machine keeps.
    bool SetText(QcMachine &machine, const std::int32_t entity, const std::string_view text) const
      requires (type == ProgsType::String)
    {
      const std::span<QcCell> cells = FindCells(machine, entity);
      if (cells.empty()) { return false; }

      cells[0] = QcCell::OfInteger(machine.AddString(text));
      return true;
    }
  };
} // quake

#endif //QUAKE_QC_FIELD_HPP
