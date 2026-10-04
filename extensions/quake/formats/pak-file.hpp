#ifndef QUAKE_PAK_FILE_HPP
#define QUAKE_PAK_FILE_HPP

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "pak-entry.hpp"
#include "pak-header.hpp"

namespace quake
{
  /// A pak file, the archive the game keeps its data in, such as `pak0.pak`:
  /// a header, the bytes of the files one after another, and a directory
  /// that names each file and says where its bytes are.
  ///
  /// It is read from the bytes of the whole file in memory, and copies none
  /// of them: the bytes of a file inside are handed out as a part of the
  /// bytes it was read from. So those have to stay where they are for as
  /// long as the pak, or anything it handed out, is used.
  ///
  /// Names are compared exactly, byte for byte: `maps/e1m1.bsp` is not
  /// `MAPS/E1M1.BSP` and not `maps\e1m1.bsp`. The game compares them the
  /// same way, its own names are all lower case with forward slashes, and
  /// Neon Engine holds that the case of a letter matters everywhere, so that
  /// what works on one system works on all. A name is kept as the directory
  /// has it and is never changed.
  class PakFile
  {
    std::span<const std::uint8_t> _bytes;
    PakHeader _header;
    std::vector<PakEntry> _entries;

  public:
    /// Takes the header and the directory from the bytes of a whole pak
    /// file. Returns false, says what was wrong in `error`, and stays as it
    /// was, when the bytes are not a pak file or point outside themselves.
    ///
    /// Everything is checked here, once: after a pak was read, every entry
    /// of it lies inside the bytes.
    bool Read(std::span<const std::uint8_t> bytes, std::string &error);

    [[nodiscard]] const PakHeader &GetHeader() const;

    /// The directory, in the order of the file.
    [[nodiscard]] const std::vector<PakEntry> &GetEntries() const;

    /// The entry of a name, or `nullptr` when the pak has no such file. Of
    /// two entries with one name the first is found, as in the game.
    [[nodiscard]] const PakEntry *Find(std::string_view name) const;

    /// Whether the pak has a file of that name.
    [[nodiscard]] bool Has(std::string_view name) const;

    /// The names of the files, in the order of the directory. With a
    /// `folder`, only those under it: `maps` and `maps/` both give
    /// `maps/e1m1.bsp`, and neither gives `mapsource/a.map`.
    [[nodiscard]] std::vector<std::string> ListNames(std::string_view folder = {}) const;

    /// The bytes of a file, without copying them. Empty for an entry that is
    /// not one of this pak and lies outside it.
    [[nodiscard]] std::span<const std::uint8_t> GetBytes(const PakEntry &entry) const;

    /// The bytes of the file of that name, without copying them. Empty when
    /// the pak has no such file, and for a file of no bytes, which Has()
    /// tells apart.
    [[nodiscard]] std::span<const std::uint8_t> GetBytes(std::string_view name) const;
  };
} // quake

#endif //QUAKE_PAK_FILE_HPP
