#ifndef QUAKE_QC_GLOBAL_HPP
#define QUAKE_QC_GLOBAL_HPP

#include <array>
#include <cstdint>
#include <string_view>
#include <type_traits>

#include "formats/progs-definition.hpp"
#include "formats/progs.hpp"
#include "formats/qc-machine.hpp"

namespace quake
{
  /// Where one global of the game code is: found once by its name, then read
  /// and written on a machine without a search.
  ///
  /// The type is the one the game code gives the global. A float is a
  /// `float`, a vector three of them, and a string, an entity, and a
  /// function are the integer the machine has for them. A name the program
  /// does not have, or has with another type, leaves the place not found:
  /// it then reads as zero and refuses a write, so that a host need not ask
  /// before each use.
  template <ProgsType type>
  class QcGlobal final
  {
    /// The offset among the globals, or -1, which the machine reads as zero
    /// and refuses to write.
    std::int32_t _offset = -1;

  public:
    /// What Get() gives and Set() takes.
    using Value = std::conditional_t<
      type == ProgsType::Float, float,
      std::conditional_t<type == ProgsType::Vector, std::array<float, 3>, std::int32_t>>;

    /// A place that is not found.
    QcGlobal() = default;

    QcGlobal(const Progs &progs, const std::string_view name)
    {
      const ProgsDefinition *definition = progs.FindGlobal(name);
      if (definition != nullptr && definition->GetType() == type) { _offset = definition->offset; }
    }

    /// Whether the program has a global of the name and the type.
    [[nodiscard]] bool IsFound() const
    {
      return _offset >= 0;
    }

    /// The offset among the globals, or -1.
    [[nodiscard]] std::int32_t GetOffset() const
    {
      return _offset;
    }

    [[nodiscard]] Value Get(const QcMachine &machine) const
    {
      if constexpr (type == ProgsType::Float) { return machine.GetFloat(_offset); }
      else if constexpr (type == ProgsType::Vector) { return machine.GetVector(_offset); }
      else { return machine.GetInteger(_offset); }
    }

    bool Set(QcMachine &machine, const Value &value) const
    {
      if constexpr (type == ProgsType::Float) { return machine.SetFloat(_offset, value); }
      else if constexpr (type == ProgsType::Vector) { return machine.SetVector(_offset, value); }
      else { return machine.SetInteger(_offset, value); }
    }

    /// The text of a global that is a string. Empty when it is not found.
    [[nodiscard]] std::string_view GetText(const QcMachine &machine) const requires (type == ProgsType::String)
    {
      return machine.GetString(machine.GetInteger(_offset));
    }

    /// Sets a global that is a string to a text, which the machine keeps.
    bool SetText(QcMachine &machine, const std::string_view text) const requires (type == ProgsType::String)
    {
      return IsFound() && machine.SetInteger(_offset, machine.AddString(text));
    }
  };
} // quake

#endif //QUAKE_QC_GLOBAL_HPP
