// The extension: what it brings to the engine, and where it starts. It reads
// the data of the game, shows a level of it, and runs the game code for it.

#include <cstdlib>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <neon/extension/neon-extension.hpp>

#include "game-code.hpp"
#include "game-data.hpp"
#include "level-view.hpp"
#include "model-view.hpp"
#include "sound-view.hpp"
#include "start-screen.hpp"
#include "game/basedir-choice.hpp"
#include "game/basedir-file.hpp"
#include "game/basedir-list.hpp"

namespace quake
{
  /// Lets time pass for the game: a step of the game code for every step of
  /// the world, and the models that play by themselves in every frame.
  /// Without game code it puts the player where the level says one starts,
  /// once the scene has one, which is after the level was shown.
  class GameRunning final : public neon::extension::System
  {
    LevelView *_level;
    ModelView *_models;
    GameCode *_code;
    double _time = 0.0;

  public:
    GameRunning(LevelView *level, ModelView *models, GameCode *code)
    {
      _level = level;
      _models = models;
      _code = code;
    }

    void Update(neon::extension::World &world, const double delta_time) override
    {
      if (!_code->IsRunning()) { _level->PlacePlayer(world); }

      _code->ReadInput(world, static_cast<float>(delta_time));

      _time += delta_time;
      _models->Update(world, _time);
    }

    void Interpolate(neon::extension::World &world, const double blend) override
    {
      _code->Interpolate(world, static_cast<float>(blend));
    }

    void FixedUpdate(neon::extension::World &world, const double fixed_delta_time) override
    {
      _code->Advance(world, static_cast<float>(fixed_delta_time));
    }
  };

  /// Lets what is shown before the game starts take its turn in every
  /// frame: the menu that asks which data to play.
  class Starting final : public neon::extension::System
  {
    std::function<void(neon::extension::World &)> _each_frame;

  public:
    explicit Starting(std::function<void(neon::extension::World &)> each_frame)
      : _each_frame(std::move(each_frame)) {}

    void Update(neon::extension::World &world, const double delta_time) override
    {
      _each_frame(world);
    }
  };

  class Quake final : public neon::extension::Extension
  {
    GameData _data;
    LevelView _level;
    ModelView _models;
    SoundView _sounds;
    SpriteView _sprites;
    GameCode _code;
    StartScreen _screen;
    bool _has_data = false;

    // the copies of the data under assets://basedirs/, and which is played
    std::vector<Basedir> _basedirs;
    BasedirChoice _choice;

    // why the game cannot start, when it cannot
    std::string _problem;

    /// What the player chose before, from user://basedir.yml.
    static std::string ReadRemembered(const neon::extension::World &world)
    {
      std::vector<std::uint8_t> contents;
      const std::string path(BasedirFile::path);
      if (!world.FileExists(path) || !world.ReadFile(path, contents)) { return ""; }
      return BasedirFile::Read(std::string_view(reinterpret_cast<const char *>(contents.data()), contents.size()));
    }

    /// Reads the data of a copy, and keeps what the player saves of it in a
    /// folder of its own.
    void LoadData(const neon::extension::World &world, const Basedir &basedir)
    {
      std::string error;
      _has_data = _data.Load(world, basedir.data_folder, error);
      if (!_has_data)
      {
        _problem = "The data in assets/basedirs/" + basedir.name + " cannot be used: " + error;
        return;
      }

      _sounds.SetDataFolder(_data.GetFolder());
      _code.SetUserFolder(basedir.UserFolder());
      world.Info("Playing the data of " + basedir.name + ", from " + basedir.data_folder + ", kept under " +
        basedir.UserFolder());
    }

    /// Takes the copy the player picked from the menu, once there is one.
    void PickBasedir(neon::extension::World &world)
    {
      const int picked = _screen.TakePicked(world);
      if (picked < 0) { return; }

      const Basedir &basedir = _basedirs[static_cast<std::size_t>(picked)];
      const std::string text = BasedirFile::Write(basedir.name);
      world.WriteFile(std::string(BasedirFile::path), std::vector<std::uint8_t>(text.begin(), text.end()));

      LoadData(world, basedir);
      if (!_has_data)
      {
        _screen.ShowProblem(world, _problem);
        return;
      }
      StartGame(world);
    }

    /// A file the player may write to choose the level that is shown: its
    /// first line is the name of a level, such as `maps/lq_e1m1.bsp`.
    static constexpr std::string_view level_file = "assets://level.txt";

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

    /// Greets the player, or starts the level that was asked for.
    void StartGame(neon::extension::World &world)
    {
      // The game greets a player with its menu over a recording. A level
      // that was asked for by name, and a tour, start at once.
      if (ReadWantedLevel(world).empty() && std::getenv("QUAKE_TOUR") == nullptr &&
          _code.StartTitle(world, _data, _level, _models, _sounds, _sprites))
      {
        return;
      }

      const std::string map = ChooseLevel(world);
      if (std::string error; !_level.Show(world, _data, map, error))
      {
        world.Error(error);
        return;
      }

      // a level without game code is still one to walk
      if (std::string error; !_code.Start(world, _data, _level, _models, _sounds, _sprites, map, error))
      {
        world.Warn("The level is shown without the game: " + error);
      }
    }

  public:
    bool Initialize(neon::extension::World &world) override
    {
      // The copies of the data the player put under assets://basedirs/,
      // each a folder with an id1 in it, and which of them is played: the
      // only one, the one chosen before, or the one the player picks.
      _basedirs = BasedirList::Find(
        world.ListFolders(std::string(BasedirList::folder)),
        [&world](const std::string &folder) { return world.ListFolders(folder); });
      _choice = BasedirChoice::Of(_basedirs, ReadRemembered(world));
      if (!_choice.warning.empty()) { world.Warn(_choice.warning); }
      _problem = _choice.problem;

      if (_choice.chosen >= 0) { LoadData(world, _basedirs[static_cast<std::size_t>(_choice.chosen)]); }

      AddSystem<Starting>("Starting", [this](neon::extension::World &each) { PickBasedir(each); });
      AddSystem<GameRunning>("GameRunning", &_level, &_models, &_code);
      return true;
    }

    void Start(neon::extension::World &world) override
    {
      // The game does not start without its data, which is the player's own
      // and may not be there yet: it says why, and closes when asked.
      if (_choice.asks)
      {
        _screen.ShowPicker(world, _basedirs);
        return;
      }
      if (!_has_data)
      {
        _screen.ShowProblem(world, _problem);
        return;
      }

      StartGame(world);
    }
  };
} // quake

NEON_EXTENSION(quake::Quake)
