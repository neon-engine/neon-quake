#ifndef QUAKE_PROGS_STATEMENT_HPP
#define QUAKE_PROGS_STATEMENT_HPP

#include <cstdint>

namespace quake
{
  /// One step of the game code: what to do, and three operands. Eight bytes
  /// in the file.
  ///
  /// An operand is the offset of a global for most opcodes, and is read
  /// without a sign then. For `If`, `IfNot`, and `Goto` one operand is a
  /// number of statements to jump, forward or back, and is read with a sign.
  /// See `ProgsOpcode` for the numbers of `opcode`.
  struct ProgsStatement
  {
    std::uint16_t opcode = 0;
    std::uint16_t a = 0;
    std::uint16_t b = 0;
    std::uint16_t c = 0;
  };
} // quake

#endif //QUAKE_PROGS_STATEMENT_HPP
