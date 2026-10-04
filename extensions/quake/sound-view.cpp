#include "sound-view.hpp"

#include <span>
#include <utility>

#include "formats/quake-space.hpp"

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

  Entity SoundView::Make(
    const World &world,
    const std::string &name,
    const std::string &path,
    const Vector3 &place,
    const float volume,
    const float far,
    const bool loops)
  {
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

        world.DestroyEntity(playing->entity);
        _playing.erase(playing);
        break;
      }
    }

    const std::string &path = Find(world, data, name);
    if (path.empty() || volume <= 0.0f) { return; }

    const float far = wears_off > 0.0f ? reach / wears_off * QuakeSpace::metres_per_unit : 0.0f;
    _playing.push_back({Make(world, name, path, place, volume, far, false), owner, channel, false});
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
    _playing.push_back({Make(world, name, path, place, volume, far, true), 0, 0, true});
  }

  void SoundView::Update(const World &world)
  {
    // the engine says that a sound has ended by no longer playing it
    const NeonField playing_field = world.FindField("SoundSource", "playing");
    std::erase_if(_playing, [&world, playing_field](const Playing &playing)
    {
      if (playing.is_ambient || world.GetBoolean(playing.entity, playing_field)) { return false; }

      world.DestroyEntity(playing.entity);
      return true;
    });
  }

  void SoundView::Clear(const World &world)
  {
    for (const Playing &playing : _playing) { world.DestroyEntity(playing.entity); }
    _playing.clear();
  }
} // quake
