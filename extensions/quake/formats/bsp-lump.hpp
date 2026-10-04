#ifndef QUAKE_BSP_LUMP_HPP
#define QUAKE_BSP_LUMP_HPP

#include <cstdint>

namespace quake
{
  /// Where one part of a level lies in its file, as the header says it.
  struct BspLump
  {
    /// Bytes from the start of the file.
    std::int32_t offset = 0;

    /// Bytes the part takes.
    std::int32_t size = 0;
  };
} // quake

#endif //QUAKE_BSP_LUMP_HPP
