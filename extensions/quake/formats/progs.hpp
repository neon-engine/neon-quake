#ifndef QUAKE_PROGS_HPP
#define QUAKE_PROGS_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "progs-definition.hpp"
#include "progs-function.hpp"
#include "progs-header.hpp"
#include "progs-statement.hpp"

namespace quake
{
  /// The game code as `progs.dat` holds it: QuakeC, compiled to statements
  /// for a small machine, with the globals and strings they work on and the
  /// names of everything. `QcMachine` runs it.
  ///
  /// The file is a header and six tables. What Read() accepts has every
  /// table inside the file and every offset a table holds inside the table
  /// it points into; what the statements do with their operands is checked
  /// by the machine while it runs them.
  struct Progs
  {
    /// The only version there is of the format of the original.
    static constexpr std::int32_t version = 6;

    /// How many bytes the header has.
    static constexpr std::size_t header_size = 60;

    /// How many globals at the start every program has for the machine's
    /// own use: nothing, the returned value, and eight parameters, of three
    /// cells each but the first.
    static constexpr std::int32_t reserved_globals = 28;

    ProgsHeader header;
    std::vector<ProgsStatement> statements;
    std::vector<ProgsDefinition> global_definitions;
    std::vector<ProgsDefinition> field_definitions;
    std::vector<ProgsFunction> functions;

    /// Every string of the program one after another, each ended by a zero.
    /// A string is named by the offset it starts at.
    std::string strings;

    /// The globals as the file has them, a cell of 32 bits each, whatever
    /// the cell means.
    std::vector<std::uint32_t> globals;

    /// Takes the program from the bytes of the file. Returns false, says
    /// what was wrong in `error`, and stays as it was, when the bytes are
    /// not a program that can be run.
    bool Read(std::span<const std::uint8_t> bytes, std::string &error);

    /// Whether a string starts at this offset.
    [[nodiscard]] bool HasString(std::int32_t offset) const;

    /// The string that starts at an offset, up to its zero. Empty when the
    /// offset is outside the strings.
    [[nodiscard]] std::string_view GetString(std::int32_t offset) const;

    /// The number of the function with a name, or nothing.
    [[nodiscard]] std::optional<std::int32_t> FindFunction(std::string_view name) const;

    /// The definition of the global with a name, or null.
    [[nodiscard]] const ProgsDefinition *FindGlobal(std::string_view name) const;

    /// The definition of the field with a name, or null.
    [[nodiscard]] const ProgsDefinition *FindField(std::string_view name) const;
  };
} // quake

#endif //QUAKE_PROGS_HPP
