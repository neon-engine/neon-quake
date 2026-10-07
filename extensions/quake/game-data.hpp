#ifndef QUAKE_GAME_DATA_HPP
#define QUAKE_GAME_DATA_HPP

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <neon/extension/neon-extension.hpp>

#include "formats/pak-layers.hpp"
#include "formats/palette.hpp"

namespace quake
{
  /// The data of the game as the player has it: the archives of the folder
  /// `id1`, one over the other, and the palette every picture is painted
  /// with. It is read once, when the extension starts, and kept, since what
  /// the archives hand out are views of their bytes.
  class GameData
  {
    // the bytes of each archive, which the archives below look into
    std::vector<std::vector<std::uint8_t>> _archives;
    PakLayers _paks;
    Palette _palette;

    // the folder id1 the data was read from, ending with a slash
    std::string _folder;

  public:
    /// Reads `pak0.pak`, `pak1.pak`, and so on from the folder `id1` of a
    /// copy of the game's data, `assets://basedirs/steam/id1/`, for as long
    /// as there is a next one, in any letter case (`PAK0.PAK` as on Steam),
    /// and the palette out of them. Returns false when there is no archive,
    /// one cannot be read, or they hold no palette, and says which in
    /// `error`.
    bool Load(const neon::extension::World &world, const std::string &folder, std::string &error);

    /// The folder the data was read from, where the files that are not in
    /// the archives are too, such as the music.
    [[nodiscard]] const std::string &GetFolder() const;

    /// The bytes of a file by the name the game knows it by, such as
    /// `maps/start.bsp`, from the last archive that has it. Empty when none
    /// has.
    [[nodiscard]] std::span<const std::uint8_t> Find(std::string_view name) const;

    /// The names of the files under a folder, each once, in their order.
    [[nodiscard]] std::vector<std::string> ListNames(std::string_view folder_name) const;

    [[nodiscard]] const Palette &GetPalette() const;
  };
} // quake

#endif //QUAKE_GAME_DATA_HPP
