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

  public:
    /// The folder the data is looked for in, under the assets of the
    /// extension next to the runtime.
    static constexpr std::string_view folder = "extensions://quake/assets/id1/";

    /// Reads `pak0.pak`, `pak1.pak`, and so on, for as long as there is a
    /// next one, in any letter case (`PAK0.PAK` as on Steam), and the
    /// palette out of them. Returns false when there is
    /// no archive, one cannot be read, or they hold no palette, and says
    /// which in `error`.
    bool Load(const neon::extension::World &world, std::string &error);

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
