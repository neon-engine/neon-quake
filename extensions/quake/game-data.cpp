#include "game-data.hpp"

#include <utility>

#include "formats/pak-file.hpp"

namespace quake
{
  bool GameData::Load(const neon::extension::World &world, std::string &error)
  {
    // the archives are counted from 0, and the game stops at the first
    // number that is missing
    for (int number = 0;; number++)
    {
      const std::string path = std::string(folder) + "pak" + std::to_string(number) + ".pak";
      if (!world.FileExists(path)) { break; }

      std::vector<std::uint8_t> bytes;
      if (!world.ReadFile(path, bytes))
      {
        error = path + " cannot be read";
        return false;
      }

      // The bytes are kept before the archive is read, since the archive
      // looks into them. Moving the list of archives later moves no bytes.
      _archives.push_back(std::move(bytes));

      PakFile pak;
      if (std::string problem; !pak.Read(_archives.back(), problem))
      {
        error = path + " is no archive of the game: " + problem;
        return false;
      }
      _paks.Add(std::move(pak));
    }

    if (_paks.GetCount() == 0)
    {
      error = "there is no " + std::string(folder) + "pak0.pak";
      return false;
    }

    if (!_palette.Read(_paks.GetBytes("gfx/palette.lmp")))
    {
      error = "the archives hold no palette, gfx/palette.lmp, of 768 bytes";
      return false;
    }

    return true;
  }

  std::span<const std::uint8_t> GameData::Find(const std::string_view name) const
  {
    return _paks.GetBytes(name);
  }

  std::vector<std::string> GameData::ListNames(const std::string_view folder_name) const
  {
    return _paks.ListNames(folder_name);
  }

  const Palette &GameData::GetPalette() const
  {
    return _palette;
  }
} // quake
