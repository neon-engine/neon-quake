#include "demo-show.hpp"

#include <cmath>
#include <cstdlib>

#include "formats/quake-space.hpp"

namespace quake
{
  using neon::extension::Entity;
  using neon::extension::World;

  // Helpers of DemoShow, for this file alone.
  namespace
  {
    /// The number of a model of the level in a name such as `*12`, or 0.
    std::size_t read_part_number(const std::string_view name)
    {
      if (name.size() < 2 || name[0] != '*') { return 0; }

      return static_cast<std::size_t>(std::strtoul(std::string(name.substr(1)).c_str(), nullptr, 10));
    }
  }

  bool DemoShow::Start(
    const World &world,
    const GameData &data,
    LevelView &view,
    ModelView &models,
    SoundView &sounds,
    SpriteView &sprites,
    const std::string &name,
    std::string &problem)
  {
    _world = &world;
    _data = &data;
    _view = &view;
    _models = &models;
    _sounds = &sounds;
    _sprites = &sprites;

    const std::span<const std::uint8_t> bytes = data.Find(name);
    if (bytes.empty())
    {
      problem = "the data has no " + name;
      return false;
    }
    return _player.Open(bytes, problem);
  }

  void DemoShow::LevelBegan(const DemoServerInfo &info)
  {
    const World &world = *_world;
    _has_level = false;
    if (info.model_names.size() < 2) { return; }

    std::string error;
    if (!_view->Show(world, *_data, info.model_names[1], error))
    {
      world.Warn("The level of a recording is not shown: " + error);
      return;
    }
    _has_level = true;

    _root = world.CreateEntity("recording " + std::to_string(++_made));
    world.AddComponent(_root, "Transform");

    // the fog of the level, for the shaders of the game, see quake.glsl
    const std::array<float, 3> fog = _view->GetFogColour();
    world.SetShaderNumbers(0, {_view->GetFogDensity(), 0.0f, 0.0f, 0.0f});
    world.SetShaderNumbers(1, {fog[0], fog[1], fog[2], 0.0f});

    // A model of the level is where the recording says, and not there
    // until it says so: a door, a lift, the bars of a gate.
    for (const std::string &model : info.model_names)
    {
      const std::size_t part = read_part_number(model);
      if (const Entity shown = part > 0 ? _view->FindPart(part) : 0; shown != 0)
      {
        world.SetVector3(shown, world.FindField("Transform", "position"), {0.0f, out_of_sight, 0.0f});
      }
    }
  }

  void DemoShow::SoundStarted(const DemoSound &sound)
  {
    if (!_has_level) { return; }

    const std::string name(_player.GetSoundName(sound.sound));
    if (name.empty()) { return; }

    const BspVector place = QuakeSpace::ToEnginePosition({sound.origin[0], sound.origin[1], sound.origin[2]});
    _sounds->Play(
      *_world, *_data, sound.entity, sound.channel, name, {place.x, place.y, place.z}, sound.volume, sound.attenuation);
  }

  void DemoShow::MusicTrackSet(const std::int32_t track, const std::int32_t loop_track)
  {
    _sounds->PlayMusic(*_world, track);
  }

  void DemoShow::Hide(Shown &shown)
  {
    if (shown.entity == 0) { return; }

    if (shown.is_part)
    {
      // it cannot be made again, so it is kept, out of sight
      _world->SetVector3(shown.entity, _world->FindField("Transform", "position"), {0.0f, out_of_sight, 0.0f});
    } else
    {
      if (shown.is_alias) { _models->Forget(shown.entity); }
      if (shown.is_sprite) { _sprites->Forget(shown.entity); }
      _world->DestroyEntity(shown.entity);
    }
    shown = {};
  }

  bool DemoShow::Show(Shown &shown, const DemoEntityState &state)
  {
    const World &world = *_world;
    const std::string model(_player.GetModelName(state.model));
    // another skin is shown by another entity, as another model is, see
    // GameCode::Show
    if (model != shown.model || (shown.is_alias && state.skin != shown.skin))
    {
      Hide(shown);
      shown.model = model;
      shown.skin = state.skin;
      if (const std::size_t part = read_part_number(model); part > 0)
      {
        shown.entity = _view->FindPart(part);
        shown.is_part = true;
      } else if (model.ends_with(".mdl") || model.ends_with(".spr") || model.ends_with(".bsp"))
      {
        shown.entity = world.CreateEntity("recorded " + std::to_string(++_made), _root);
        shown.is_alias = model.ends_with(".mdl");
        shown.is_sprite = model.ends_with(".spr");
        if (model.ends_with(".bsp"))
        {
          world.AddComponent(shown.entity, "Transform");
          _view->ShowItem(world, *_data, model, shown.entity);
        }
      }
    }
    if (shown.entity == 0) { return false; }

    const LevelVector &origin = state.origin;
    if (shown.is_alias)
    {
      const std::array<float, 3> light = model.find("flame") != std::string::npos
                                           ? std::array<float, 3>{1.0f, 1.0f, 1.0f}
                                           : _view->FindLight({origin[0], origin[1], origin[2]});
      if (!_models->Show(world, *_data, shown.entity, model, state.frame, state.skin, light))
      {
        world.DestroyEntity(shown.entity);
        shown.entity = 0;
        return false;
      }
    }
    if (shown.is_sprite && !_sprites->Show(world, *_data, shown.entity, model, state.frame))
    {
      world.DestroyEntity(shown.entity);
      shown.entity = 0;
      return false;
    }

    const BspVector place = QuakeSpace::ToEnginePosition({origin[0], origin[1], origin[2]});
    world.SetVector3(shown.entity, world.FindField("Transform", "position"), {place.x, place.y, place.z});
    if (shown.is_part)
    {
      // a button that was pressed shows the other run of its textures
      _view->SetPartFrame(read_part_number(model), state.frame);
    } else if (!shown.is_sprite)
    {
      // what a player picks up turns around by itself, as its model says
      const bool turns = shown.is_alias && (_models->GetFlags(shown.entity) & turns_flag) != 0;
      const float yaw = turns ? std::fmod(100.0f * _player.GetTime(), 360.0f) : state.angles[1];
      world.SetVector3(shown.entity, world.FindField("Transform", "rotation"), {state.angles[2], yaw, state.angles[0]});
    }
    return true;
  }

  void DemoShow::ShowView()
  {
    const World &world = *_world;
    if (_eyes == 0)
    {
      _eyes = world.FindEntity("player");
      if (_eyes == 0) { return; }
      _camera = world.FindEntity("player/camera");
    }

    const DemoEntity *seen_from = _player.FindEntity(_player.GetViewEntity());
    if (seen_from == nullptr) { return; }

    // Between levels and in a scene that is played the view is where its
    // entity is and looks where it looks. Otherwise it is at the height of
    // the eyes, and looks where the player looked.
    const bool is_held = _player.GetIntermission() != DemoIntermission::None;
    const DemoClientData &client = _player.GetClientData();
    LevelVector angles = is_held ? seen_from->state.angles : _player.GetViewAngles();
    if (!is_held)
    {
      for (std::size_t i = 0; i < 3; i++) { angles[i] += client.punch_angles[i]; }
    }

    const LevelVector &origin = seen_from->state.origin;
    const BspVector place = QuakeSpace::ToEnginePosition({origin[0], origin[1], origin[2] + client.view_height});
    world.SetVector3(_eyes, world.FindField("Transform", "position"), {place.x, place.y, place.z});
    world.SetVector3(_eyes, world.FindField("Transform", "rotation"), {0.0f, QuakeSpace::ToEngineYaw(angles[1]), 0.0f});
    if (_camera != 0)
    {
      world.SetVector3(_camera, world.FindField("Transform", "rotation"), {-angles[0], 0.0f, -angles[2]});
    }
    _sprites->Face(world, -angles[0], QuakeSpace::ToEngineYaw(angles[1]));
  }

  void DemoShow::Update(const World &world, const float seconds)
  {
    _world = &world;
    _player.Advance(seconds);
    if (!_has_level || !_player.HasLevel()) { return; }

    const double time = static_cast<double>(_player.GetTime());

    // the lights of the level, as the recording sets their styles
    for (std::size_t style = 0; style < _styles.size(); style++)
    {
      const std::string_view text = _player.GetLightStyle(style);
      if (text == _styles[style]) { continue; }

      _styles[style] = std::string(text);
      _view->SetLightStyle(static_cast<std::int32_t>(style), text);
    }
    _view->UpdateLight(world, time);
    _view->UpdateTextures(world, time);

    // what stands still for as long as the level is there: a torch
    const std::span<const DemoEntityState> statics = _player.GetStaticEntities();
    for (; _static_entities < statics.size(); _static_entities++)
    {
      Shown still;
      Show(still, statics[_static_entities]);
    }

    // what sounds without end: the hum of a lamp, the wind
    const std::span<const DemoStaticSound> sounds = _player.GetStaticSounds();
    for (; _static_sounds < sounds.size(); _static_sounds++)
    {
      const DemoStaticSound &sound = sounds[_static_sounds];
      const std::string name(_player.GetSoundName(sound.sound));
      if (name.empty()) { continue; }

      const BspVector place = QuakeSpace::ToEnginePosition({sound.origin[0], sound.origin[1], sound.origin[2]});
      _sounds->PlayAmbient(world, *_data, name, {place.x, place.y, place.z}, sound.volume, sound.attenuation);
    }

    // the entities, but the one the view is: who looks sees no body
    for (auto &[number, shown] : _shown) { shown.is_there = false; }
    for (const DemoEntity &entity : _player.GetEntities())
    {
      if (entity.number == _player.GetViewEntity() || entity.state.model == 0) { continue; }

      Shown &shown = _shown[entity.number];
      if (Show(shown, entity.state)) { shown.is_there = true; }
    }
    for (auto &[number, shown] : _shown)
    {
      if (!shown.is_there) { Hide(shown); }
    }

    _sprites->Update(world, *_data, time);
    _sounds->Update(world);
    ShowView();
  }

  bool DemoShow::IsOver() const
  {
    return _player.IsOver();
  }

  void DemoShow::Stop(const World &world)
  {
    if (_player.HasFailed()) { world.Warn("A recording was cut short: " + _player.GetProblem()); }

    _player.Close();
    _shown.clear();
    _models->Clear();
    _sprites->Clear();
    _sounds->Clear(world);
    _sounds->PlayMusic(world, 0);
    if (_root != 0) { world.DestroyEntity(_root); }
    _root = 0;
    _static_entities = 0;
    _static_sounds = 0;
    _styles = {};
    _has_level = false;
  }
} // quake
