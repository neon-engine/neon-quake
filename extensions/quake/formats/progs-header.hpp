#ifndef QUAKE_PROGS_HEADER_HPP
#define QUAKE_PROGS_HEADER_HPP

#include <cstdint>

namespace quake
{
  /// The start of `progs.dat`: which version it is, and where each of its
  /// tables is and how many entries it has. Fifteen numbers of four bytes.
  ///
  /// The strings are counted in bytes and the globals in cells of four bytes;
  /// every other table is counted in entries.
  struct ProgsHeader
  {
    std::int32_t version = 0;

    /// A checksum of the names the compiler was given for the globals and
    /// fields the engine reads, by which the original told whether a program
    /// was compiled for it.
    std::int32_t crc = 0;

    std::int32_t statements_offset = 0;
    std::int32_t statements_count = 0;
    std::int32_t global_definitions_offset = 0;
    std::int32_t global_definitions_count = 0;
    std::int32_t field_definitions_offset = 0;
    std::int32_t field_definitions_count = 0;
    std::int32_t functions_offset = 0;
    std::int32_t functions_count = 0;
    std::int32_t strings_offset = 0;
    std::int32_t strings_size = 0;
    std::int32_t globals_offset = 0;
    std::int32_t globals_count = 0;

    /// How many cells of four bytes an entity has for its fields.
    std::int32_t entity_fields = 0;
  };
} // quake

#endif //QUAKE_PROGS_HEADER_HPP
