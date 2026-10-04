// The extension: what it brings to the engine, and where it starts. It reads
// the data of the game and shows a level of it.

#include <string>
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

      // where the game itself starts, or the first level there is
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

      if (std::string error; !_level.Show(world, _data, map, error)) { world.Error(error); }
    }
  };
} // quake

NEON_EXTENSION(quake::Quake)
