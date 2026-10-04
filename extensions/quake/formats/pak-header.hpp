#ifndef QUAKE_PAK_HEADER_HPP
#define QUAKE_PAK_HEADER_HPP

#include <cstddef>
#include <string_view>

namespace quake
{
  /// The start of a pak file: four letters that say what it is, then where
  /// its directory starts and how many bytes the directory has, each a
  /// number of four bytes.
  ///
  /// The file keeps both numbers with a sign. They are held here as they are
  /// once PakFile has refused the negative ones.
  struct PakHeader
  {
    /// The four letters every pak file starts with.
    static constexpr std::string_view magic = "PACK";

    /// How many bytes the header has in the file.
    static constexpr std::size_t size_in_bytes = 12;

    /// Where the directory starts, counted from the start of the file.
    std::size_t directory_offset = 0;

    /// How many bytes the directory has: 64 for each file in the pak.
    std::size_t directory_size = 0;
  };
} // quake

#endif //QUAKE_PAK_HEADER_HPP
