#ifndef QUAKE_PAK_ENTRY_HPP
#define QUAKE_PAK_ENTRY_HPP

#include <cstddef>
#include <string>

namespace quake
{
  /// One line of the directory of a pak file: the name of a file inside it,
  /// and where its bytes are.
  ///
  /// The name is a path with forward slashes, such as `maps/e1m1.bsp`, kept
  /// in 56 bytes and ended by a zero when it is shorter.
  struct PakEntry
  {
    /// How many bytes the name has in the file.
    static constexpr std::size_t name_size = 56;

    /// How many bytes an entry has in the file: the name and two numbers of
    /// four bytes.
    static constexpr std::size_t size_in_bytes = 64;

    std::string name;

    /// Where the bytes of the file start, counted from the start of the pak.
    std::size_t offset = 0;

    /// How many bytes the file has.
    std::size_t size = 0;
  };
} // quake

#endif //QUAKE_PAK_ENTRY_HPP
