// The extension: what it brings to the engine, and where it starts. It reads
// the data of the game and shows a level of it.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <neon/extension/neon-extension.hpp>

#include "game-data.hpp"
#include "level-view.hpp"

namespace quake
{
  /// Puts the camera where a player starts once the scene has one, which is
  /// after the level was shown.
  class CameraPlacing final : public neon::extension::System
  {
    LevelView *_level;

  public:
    explicit CameraPlacing(LevelView *level) { _level = level; }

    void Update(neon::extension::World &world, const double delta_time) override
    {
      _level->PlaceCamera(world);
    }
  };

  class Quake final : public neon::extension::Extension
  {
    GameData _data;
    LevelView _level;
    bool _has_data = false;

    /// A file the player may write to choose the level that is shown: its
    /// first line is the name of a level, such as `maps/lq_e1m1.bsp`.
    static constexpr std::string_view level_file = "extensions://quake/assets/level.txt";

    /// The level the file `level.txt` asks for, in its first line, or
    /// nothing when there is no such file or it says nothing.
    static std::string ReadWantedLevel(const neon::extension::World &world)
    {
      std::vector<std::uint8_t> contents;
      if (!world.FileExists(std::string(level_file)) || !world.ReadFile(std::string(level_file), contents)) { return ""; }

      // the first line, without the blanks around it
      std::string line(contents.begin(), std::find(contents.begin(), contents.end(), '\n'));
      const std::size_t first = line.find_first_not_of(" \t\r");
      if (first == std::string::npos) { return ""; }
      return line.substr(first, line.find_last_not_of(" \t\r") - first + 1);
    }

    /// Which level is shown: the one `level.txt` asks for when the data has
    /// it, or else where the game itself starts, or the first level there
    /// is.
    std::string ChooseLevel(const neon::extension::World &world) const
    {
      if (const std::string wanted = ReadWantedLevel(world); !wanted.empty())
      {
        if (!_data.Find(wanted).empty()) { return wanted; }
        world.Warn("The data of the game holds no " + wanted + ", which " + std::string(level_file) +
          " asks for, so the level the game starts with is shown");
      }

      std::string map = "maps/start.bsp";
      if (_data.Find(map).empty())
      {
        for (const std::string &name : _data.ListNames("maps"))
        {
          if (name.ends_with(".bsp"))
          {
            map = name;
            break;
          }
        }
      }
      return map;
    }

  public:
    bool Initialize(neon::extension::World &world) override
    {
      // The game starts without its data and says so, since the data is the
      // player's own and may not be there yet.
      std::string error;
      _has_data = _data.Load(world, error);
      if (!_has_data) { world.Warn("The data of the game is not there: " + error + ". See README.md"); }

      AddSystem<CameraPlacing>("CameraPlacing", &_level);
      return true;
    }

    void Start(neon::extension::World &world) override
    {
      if (!_has_data) { return; }

      const std::string map = ChooseLevel(world);
      if (std::string error; !_level.Show(world, _data, map, error)) { world.Error(error); }
    }
  };
} // quake

NEON_EXTENSION(quake::Quake)
