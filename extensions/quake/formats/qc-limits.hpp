#ifndef QUAKE_QC_LIMITS_HPP
#define QUAKE_QC_LIMITS_HPP

#include <cstddef>
#include <cstdint>

namespace quake
{
  /// How far `QcMachine` lets a program go before it stops it with an error.
  /// The original had the same limits with smaller numbers: 32 calls deep,
  /// 2048 saved locals, 100000 statements, 600 entities.
  struct QcLimits
  {
    /// How many calls may be inside one another.
    std::size_t stack_depth = 256;

    /// How many cells of locals may be kept aside for the calls that are
    /// under way.
    std::size_t saved_locals = 16384;

    /// How many statements one call from outside may run, with everything
    /// it calls. A program that runs more is taken to be stuck in a loop.
    std::int64_t statements = 1000000;

    /// How many entities there may be, the world among them.
    std::int32_t entities = 8192;
  };
} // quake

#endif //QUAKE_QC_LIMITS_HPP
