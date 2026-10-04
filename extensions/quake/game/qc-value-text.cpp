#include "qc-value-text.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>

#include "formats/progs-function.hpp"
#include "formats/progs.hpp"

namespace quake
{
  // Helpers of QcValueText: texts taken apart and numbers put into text.
  namespace
  {
    /// A text as the game code is to see it: the two characters `\n` are
    /// one new line.
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

    /// A float as `%f` of C prints it.
    std::string PrintFloat(const float value)
    {
      // the largest float has 39 digits before the point
      std::array<char, 64> text{};
      const int length = std::snprintf(text.data(), text.size(), "%f", static_cast<double>(value));
      return length > 0 ? std::string(text.data(), static_cast<std::size_t>(length)) : std::string();
    }
  }

  std::size_t QcValueText::GetSize(const ProgsType type)
  {
    return type == ProgsType::Vector ? 3 : 1;
  }

  bool QcValueText::Parse(QcMachine &machine, const ProgsType type, const std::string_view text, Cells &cells)
  {
    const Progs &progs = machine.GetProgs();
    switch (type)
    {
      case ProgsType::String:
      {
        cells[0] = QcCell::OfInteger(machine.AddString(WithNewLines(text)));
        return true;
      }
      case ProgsType::Float:
      {
        cells[0] = QcCell::OfFloat(std::strtof(std::string(text).c_str(), nullptr));
        return true;
      }
      case ProgsType::Vector:
      {
        const std::array<float, 3> vector = ParseVector(std::string(text));
        for (std::size_t i = 0; i < vector.size(); i++) { cells[i] = QcCell::OfFloat(vector[i]); }
        return true;
      }
      case ProgsType::Entity:
      {
        cells[0] = QcCell::OfInteger(std::atoi(std::string(text).c_str()));
        return true;
      }
      case ProgsType::Field:
      {
        const ProgsDefinition *named = progs.FindField(text);
        if (named == nullptr) { return false; }
        cells[0] = QcCell::OfInteger(named->offset);
        return true;
      }
      case ProgsType::Function:
      {
        const std::optional<std::int32_t> function = progs.FindFunction(text);
        if (!function) { return false; }
        cells[0] = QcCell::OfInteger(*function);
        return true;
      }
      default:
      {
        return false;
      }
    }
  }

  std::string QcValueText::Print(const QcMachine &machine, const ProgsType type, const std::span<const QcCell> cells)
  {
    if (cells.size() < GetSize(type)) { return {}; }

    const Progs &progs = machine.GetProgs();
    switch (type)
    {
      case ProgsType::String:
      {
        return std::string(machine.GetString(cells[0].AsInteger()));
      }
      case ProgsType::Float:
      {
        return PrintFloat(cells[0].AsFloat());
      }
      case ProgsType::Vector:
      {
        return PrintFloat(cells[0].AsFloat()) + " " + PrintFloat(cells[1].AsFloat()) + " " +
          PrintFloat(cells[2].AsFloat());
      }
      case ProgsType::Entity:
      {
        return std::to_string(cells[0].AsInteger());
      }
      case ProgsType::Field:
      {
        // the first of an offset is the field itself: the parts of a
        // vector, `origin_x`, come after it
        for (const ProgsDefinition &definition : progs.field_definitions)
        {
          if (definition.offset == cells[0].AsInteger()) { return std::string(progs.GetString(definition.name)); }
        }
        return {};
      }
      case ProgsType::Function:
      {
        const std::int32_t function = cells[0].AsInteger();
        if (function < 0 || static_cast<std::size_t>(function) >= progs.functions.size()) { return {}; }
        return std::string(progs.GetString(progs.functions[static_cast<std::size_t>(function)].name));
      }
      default:
      {
        return {};
      }
    }
  }
} // quake
