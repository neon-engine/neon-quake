#include "sound-view.hpp"

#include <optional>
#include <span>
#include <utility>

#include "formats/quake-space.hpp"
#include "game/any-case-name.hpp"

namespace quake
{
  using neon::extension::Entity;
  using neon::extension::Vector3;
  using neon::extension::World;

  const std::string &SoundView::Find(const World &world, const GameData &data, const std::string &name)
  {
    if (const auto known = _paths.find(name); known != _paths.end()) { return known->second; }

    std::string &path = _paths[name];

    const std::span<const std::uint8_t> bytes = data.Find("sound/" + name);
    if (bytes.empty())
    {
      world.Warn("The data of the game holds no sound " + name);
      return path;
    }

    path = world.SetSound(name, std::vector<std::uint8_t>(bytes.begin(), bytes.end()));
    return path;
  }

  std::string SoundView::KeyOf(const std::string &path, const float far, const bool loops)
  {
    return path + "|" + std::to_string(far) + (loops ? "|loops" : "|once");
  }

  void SoundView::Rest(const World &world, const Playing &playing)
  {
    // turned off, it is stopped and kept, and plays from its start when it
    // is turned on
    world.SetEnabled(playing.entity, world.FindComponent("SoundSource"), false);
    _waiting[playing.key].push_back(playing.entity);
  }

  Entity SoundView::Make(
    const World &world,
    const std::string &name,
    const std::string &path,
    const Vector3 &place,
    const float volume,
    const float far,
    const bool loops,
    const std::string &key)
  {
    // one that waits for this sound, where it sounds now
    if (const auto waiting = _waiting.find(key); waiting != _waiting.end() && !waiting->second.empty())
    {
      const Entity entity = waiting->second.back();
      waiting->second.pop_back();

      world.SetVector3(entity, world.FindField("Transform", "position"), place);
      world.SetNumber(entity, world.FindField("SoundSource", "volume"), volume);
      world.SetBoolean(entity, world.FindField("SoundSource", "playing"), true);
      world.SetEnabled(entity, world.FindComponent("SoundSource"), true);
      return entity;
    }

    // An entity is known by its name, and the same sound may play many times
    // at once, so each gets a number.
    const Entity entity = world.CreateEntity("sound " + std::to_string(++_made) + " " + name);
    world.AddComponent(entity, "Transform");
    world.SetVector3(entity, world.FindField("Transform", "position"), place);

    world.AddComponent(entity, "SoundSource");
    world.SetText(entity, world.FindField("SoundSource", "sound"), path);
    world.SetNumber(entity, world.FindField("SoundSource", "volume"), volume);
    world.SetBoolean(entity, world.FindField("SoundSource", "looping"), loops);
    world.SetText(entity, world.FindField("SoundSource", "group"), loops ? "ambience" : "effects");

    if (far > 0.0f)
    {
      world.SetBoolean(entity, world.FindField("SoundSource", "spatial"), true);
      world.SetText(entity, world.FindField("SoundSource", "falloff"), "linear");
      world.SetNumber(entity, world.FindField("SoundSource", "min_distance"), 0.0);
      world.SetNumber(entity, world.FindField("SoundSource", "max_distance"), far);
    }
    return entity;
  }

  void SoundView::Play(
    const World &world,
    const GameData &data,
    const std::int32_t owner,
    const std::int32_t channel,
    const std::string &name,
    const Vector3 &place,
    const float volume,
    const float wears_off)
  {
    // what plays on the channel is cut short, whatever takes its place
    if (channel != 0)
    {
      for (auto playing = _playing.begin(); playing != _playing.end(); ++playing)
      {
        if (playing->is_ambient || playing->owner != owner || playing->channel != channel) { continue; }

        Rest(world, *playing);
        _playing.erase(playing);
        break;
      }
    }

    const std::string &path = Find(world, data, name);
    if (path.empty() || volume <= 0.0f) { return; }

    const float far = wears_off > 0.0f ? reach / wears_off * QuakeSpace::metres_per_unit : 0.0f;
    const std::string key = KeyOf(path, far, false);
    _playing.push_back({Make(world, name, path, place, volume, far, false, key), owner, channel, false, key});
  }

  void SoundView::PlayAmbient(
    const World &world,
    const GameData &data,
    const std::string &name,
    const Vector3 &place,
    const float volume,
    const float wears_off)
  {
    const std::string &path = Find(world, data, name);
    if (path.empty() || volume <= 0.0f) { return; }

    // A sound of the surroundings carries as far as any other. The game code
    // has it wear off three times as fast as an ordinary sound, so that it
    // is heard in its room and not in the level.
    const float far = wears_off > 0.0f ? reach / wears_off * QuakeSpace::metres_per_unit : 0.0f;
    const std::string key = KeyOf(path, far, true);
    _playing.push_back({Make(world, name, path, place, volume, far, true, key), 0, 0, true, key});
  }

  void SoundView::SetDataFolder(const std::string &folder)
  {
    _data_folder = folder;
  }

  void SoundView::PlayMusic(const World &world, const int track)
  {
    if (track == _track && _music != 0) { return; }

    if (_music != 0) { world.DestroyEntity(_music); }
    _music = 0;
    _track = track;
    if (track <= 0) { return; }

    // the tracks are files of their own, which the audio reads as they are
    const std::string number = (track < 10 ? "0" : "") + std::to_string(track);
    // found in any case, as the archives are: a copy of the music may name
    // it Track02.OGG, and the engine opens a file only by its name on disk
    if (_data_folder.empty()) { return; }
    const std::string music = _data_folder + "music/";
    const std::optional<std::string> name = FindAnyCaseName(world.ListFiles(music), "track" + number + ".ogg");
    if (!name) { return; }
    const std::string path = music + *name;

    _music = world.CreateEntity("music " + number);
    world.AddComponent(_music, "SoundSource");
    world.SetText(_music, world.FindField("SoundSource", "sound"), path);
    world.SetBoolean(_music, world.FindField("SoundSource", "looping"), true);
    world.SetText(_music, world.FindField("SoundSource", "group"), "music");
  }

  void SoundView::SetVolumes(const World &world, const float sounds, const float music)
  {
    // the groups the sounds of the game are in, see Make() and PlayMusic()
    world.SetGroupVolume("effects", sounds);
    world.SetGroupVolume("ambience", sounds);
    world.SetGroupVolume("music", music);
  }

  void SoundView::Update(const World &world)
  {
    // the engine says that a sound has ended by no longer playing it
    const NeonField playing_field = world.FindField("SoundSource", "playing");
    std::erase_if(_playing, [this, &world, playing_field](const Playing &playing)
    {
      if (playing.is_ambient || world.GetBoolean(playing.entity, playing_field)) { return false; }

      Rest(world, playing);
      return true;
    });
  }

  void SoundView::Clear(const World &world)
  {
    for (const Playing &playing : _playing) { Rest(world, playing); }
    _playing.clear();
  }
} // quake
