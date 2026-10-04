#ifndef QUAKE_PROGS_FUNCTION_HPP
#define QUAKE_PROGS_FUNCTION_HPP

#include <array>
#include <cstdint>

namespace quake
{
  /// A function of the game code. 36 bytes in the file.
  ///
  /// Its locals are globals like any other, `locals_count` cells from
  /// `first_local`, and its parameters are the first of them.
  struct ProgsFunction
  {
    /// The most parameters a function takes.
    static constexpr std::int32_t max_parameters = 8;

    /// The statement it starts at. A negative number says the function is
    /// not in the file but built into the engine, and is the number of that
    /// builtin with the sign turned.
    std::int32_t first_statement = 0;

    std::int32_t first_local = 0;

    std::int32_t locals_count = 0;

    /// A counter the original kept while running. Nothing in the file.
    std::int32_t profile = 0;

    /// Where its name starts in the strings.
    std::int32_t name = 0;

    /// Where the name of the source file it came from starts in the strings.
    std::int32_t file = 0;

    /// How many parameters it takes. Negative for a builtin that takes any
    /// number.
    std::int32_t parameters_count = 0;

    /// How many cells each parameter is: three for a vector, one for
    /// anything else.
    std::array<std::uint8_t, 8> parameter_sizes{};

    [[nodiscard]] bool IsBuiltin() const
    {
      return first_statement < 0;
    }
  };
} // quake

#endif //QUAKE_PROGS_FUNCTION_HPP
