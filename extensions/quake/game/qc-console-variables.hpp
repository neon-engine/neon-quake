#ifndef QUAKE_QC_CONSOLE_VARIABLES_HPP
#define QUAKE_QC_CONSOLE_VARIABLES_HPP

#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <string_view>

namespace quake
{
  /// The console variables the game code reads with `cvar` and writes with
  /// `cvar_set`: a name and a text each, which is read as a number.
  ///
  /// It starts with the variables the game code of the original reads, at
  /// the values the original starts them with: how hard the game is, the
  /// kind of game, how the players move. It only holds them. The host sets
  /// what a player chose before a level starts, and reads what the game code
  /// set. A variable nobody set reads as empty, and as zero.
  class QcConsoleVariables
  {
    /// In the order of the names, and found by a name without copying it.
    std::map<std::string, std::string, std::less<>> _texts;

  public:
    QcConsoleVariables();

    /// Whether a variable of that name was ever set.
    [[nodiscard]] bool Has(std::string_view name) const;

    /// The text of a variable. Empty when there is none of that name.
    [[nodiscard]] std::string_view GetText(std::string_view name) const;

    /// The text of a variable read as a number, as far as it is one: `2`
    /// and `2.5 or so` are 2 and 2.5, and what does not start with a number
    /// is zero.
    [[nodiscard]] float GetFloat(std::string_view name) const;

    /// Sets a variable, which is made when there was none of that name.
    void Set(std::string_view name, std::string_view text);

    /// Sets a variable to a number, written as short as reads back the same.
    void SetFloat(std::string_view name, float value);

    /// How many variables there are.
    [[nodiscard]] std::size_t GetCount() const;
  };
} // quake

#endif //QUAKE_QC_CONSOLE_VARIABLES_HPP
