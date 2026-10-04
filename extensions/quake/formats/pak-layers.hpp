#ifndef QUAKE_PAK_LAYERS_HPP
#define QUAKE_PAK_LAYERS_HPP

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "pak-file.hpp"

namespace quake
{
  /// Several pak files searched as one, the way the game layers them: a
  /// file is looked for in the pak that was added last first, so that
  /// `pak1.pak` wins over `pak0.pak` where both have a file of one name, and
  /// what only an earlier pak has is still found.
  ///
  /// The paks are added in the game's order, `pak0.pak` first. Names are
  /// compared as PakFile compares them, exactly. As with a PakFile, the
  /// bytes each pak was read from have to stay where they are.
  class PakLayers
  {
    std::vector<PakFile> _paks;

  public:
    /// Adds a pak on top of those added before it.
    void Add(PakFile pak);

    /// How many paks were added.
    [[nodiscard]] std::size_t GetCount() const;

    /// The pak a file of that name is taken from, the last added of those
    /// that have it, or `nullptr` when none has.
    [[nodiscard]] const PakFile *FindPak(std::string_view name) const;

    /// Whether any pak has a file of that name.
    [[nodiscard]] bool Has(std::string_view name) const;

    /// The bytes of the file of that name, without copying them, from the
    /// pak that wins. Empty when no pak has such a file, and for a file of
    /// no bytes, which Has() tells apart.
    [[nodiscard]] std::span<const std::uint8_t> GetBytes(std::string_view name) const;

    /// The names of the files of all paks, each once, sorted. With a
    /// `folder`, only those under it, as PakFile::ListNames() does.
    [[nodiscard]] std::vector<std::string> ListNames(std::string_view folder = {}) const;
  };
} // quake

#endif //QUAKE_PAK_LAYERS_HPP
