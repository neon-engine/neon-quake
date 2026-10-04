#ifndef QUAKE_QC_VALUE_TEXT_HPP
#define QUAKE_QC_VALUE_TEXT_HPP

#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>

#include "formats/progs-definition.hpp"
#include "formats/qc-cell.hpp"
#include "formats/qc-machine.hpp"

namespace quake
{
  /// A value of the game code as a text and back, by its type, as the
  /// original has it in the two places it keeps values as text: the
  /// entities of a level and a saved game.
  ///
  /// - A float is a number, and a vector three of them with spaces between.
  /// - A string is its text. Read, the two characters `\n` are a new line.
  /// - An entity is its number.
  /// - A function and a field are their names.
  ///
  /// Nothing else can be a text: what is of no type and a pointer are
  /// refused by Parse() and are empty from Print().
  class QcValueText final
  {
  public:
    /// The cells of one value. A vector fills all three, anything else the
    /// first.
    using Cells = std::array<QcCell, 3>;

    /// How many cells a value of a type is.
    [[nodiscard]] static std::size_t GetSize(ProgsType type);

    /// Makes the cells of a value from its text. A number that is none is
    /// zero, and so is what a vector lacks, as in the original. False, with
    /// the cells as they were, for a function or a field the program does
    /// not have, and for a type that has no text.
    ///
    /// A string is kept by the machine, which is why it is not const.
    static bool Parse(QcMachine &machine, ProgsType type, std::string_view text, Cells &cells);

    /// The text of a value, as the original writes it into a saved game: a
    /// float with six places after the point, `%f` of C. `cells` are as
    /// many as GetSize() says, or more. Empty for a function or a field
    /// that there is not, and for a type that has no text.
    [[nodiscard]] static std::string Print(const QcMachine &machine, ProgsType type, std::span<const QcCell> cells);
  };
} // quake

#endif //QUAKE_QC_VALUE_TEXT_HPP
