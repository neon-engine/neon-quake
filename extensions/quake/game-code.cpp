#include "game-code.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <span>
#include <utility>

#include "formats/entity-text.hpp"
#include "formats/progs.hpp"
#include "formats/quake-space.hpp"
#include "game/client-think.hpp"
#include "game/level-spawning.hpp"
#include "game/qc-builtin-number.hpp"
#include "game/qc-move-type.hpp"
#include "game/qc-solid.hpp"

namespace quake
{
  // Helpers of GameCode, for this file alone.
  namespace
  {
    using neon::extension::Entity;
    using neon::extension::World;

    /// The effect of a model that has it turn where it lies, as a weapon to
    /// pick up does.
    constexpr std::uint32_t turns_flag = 8;

    /// How far such a model turns in a second, in degrees.
    constexpr double turn_speed = 100.0;

    /// An angle in degrees, from 0 up to 360.
    float wrap_angle(const float angle)
    {
      const float wrapped = std::fmod(angle, 360.0f);
      return wrapped < 0.0f ? wrapped + 360.0f : wrapped;
    }

    /// The number of a model of the level a name such as `*3` stands for,
    /// or 0 when the name is no such one.
    std::size_t read_part_number(const std::string_view name)
    {
      if (name.size() < 2 || name[0] != '*') { return 0; }

      std::size_t number = 0;
      for (const char digit : name.substr(1))
      {
        if (digit < '0' || digit > '9' || number > 1000000) { return 0; }
        number = number * 10 + static_cast<std::size_t>(digit - '0');
      }
      return number;
    }

    /// What the game code prints, without the line end it closes with, and
    /// with the letters of the second half of the game's letters, which are
    /// the same in another colour, as the plain ones.
    std::string to_plain(const std::string_view text)
    {
      std::string plain;
      for (const char letter : text)
      {
        const auto code = static_cast<unsigned char>(letter) & 0x7fu;
        if (code == '\n') { plain += ' '; } else if (code >= 32) { plain += static_cast<char>(code); }
      }
      while (!plain.empty() && plain.back() == ' ') { plain.pop_back(); }
      return plain;
    }
  }

  GameCode::Level::Level(Progs progs, QcHost &host, LevelMover &mover)
    : machine(std::move(progs)),
      globals(machine.GetProgs()),
      fields(machine.GetProgs()),
      builtins(host),
      running(machine, &mover)
  {
  }

  void GameCode::Link(const std::int32_t entity)
  {
    QcMachine &machine = _level->machine;
    const QcFields &fields = _level->fields;

    const Vector origin = fields.origin.Get(machine, entity);
    const Vector mins = fields.mins.Get(machine, entity);
    const Vector maxs = fields.maxs.Get(machine, entity);
    fields.absmin.Set(machine, entity, {origin[0] + mins[0] - 1.0f, origin[1] + mins[1] - 1.0f, origin[2] + mins[2] - 1.0f});
    fields.absmax.Set(machine, entity, {origin[0] + maxs[0] + 1.0f, origin[1] + maxs[1] + 1.0f, origin[2] + maxs[2] + 1.0f});
  }

  void GameCode::SetModel(const std::int32_t entity, const std::int32_t name_offset, const std::string_view name)
  {
    QcMachine &machine = _level->machine;
    const QcFields &fields = _level->fields;

    // The original stops the game for a model that was not announced. Game
    // code written for it never does that, so the model is taken as it is.
    QcPrecacheList &models = _level->builtins.GetModels();
    const std::int32_t index = name.empty() ? 0 : models.Find(name).value_or(-1);
    fields.model.Set(machine, entity, name_offset);
    fields.modelindex.Set(machine, entity, static_cast<float>(index >= 0 ? index : models.Add(name)));

    // a model of the level has the size the level gives it; any other gets
    // its size from the game code, with `setsize`
    Vector mins{};
    Vector maxs{};
    if (const std::size_t part = read_part_number(name); part > 0 && part < _level->file.models.size())
    {
      const BspModel &model = _level->file.models[part];
      mins = {model.mins.x, model.mins.y, model.mins.z};
      maxs = {model.maxs.x, model.maxs.y, model.maxs.z};
    }
    fields.mins.Set(machine, entity, mins);
    fields.maxs.Set(machine, entity, maxs);
    fields.size.Set(machine, entity, {maxs[0] - mins[0], maxs[1] - mins[1], maxs[2] - mins[2]});
    Link(entity);
  }

  void GameCode::MakeStatic(const std::int32_t entity)
  {
    if (entity <= 0 || static_cast<std::size_t>(entity) >= _shown.size() && entity >= _level->machine.GetEntityCount()) { return; }

    // what it shows is shown now, and no longer kept track of, so that it
    // stays when the entity goes
    Show(entity);
    if (static_cast<std::size_t>(entity) < _shown.size()) { _shown[entity] = {}; }
    _level->builtins.RemoveEntity(_level->machine, entity);
  }

  void GameCode::RegisterBuiltins()
  {
    QcMachine &machine = _level->machine;
    const auto set = [&machine](const QcBuiltinNumber number, QcMachine::Builtin builtin)
    {
      machine.SetBuiltin(static_cast<std::int32_t>(number), std::move(builtin));
    };

    set(QcBuiltinNumber::SetOrigin, [this](QcMachine &machine)
    {
      const std::int32_t entity = machine.GetParameterInteger(0);
      _level->fields.origin.Set(machine, entity, machine.GetParameterVector(1));
      Link(entity);
    });

    set(QcBuiltinNumber::SetSize, [this](QcMachine &machine)
    {
      const std::int32_t entity = machine.GetParameterInteger(0);
      const Vector mins = machine.GetParameterVector(1);
      const Vector maxs = machine.GetParameterVector(2);
      _level->fields.mins.Set(machine, entity, mins);
      _level->fields.maxs.Set(machine, entity, maxs);
      _level->fields.size.Set(machine, entity, {maxs[0] - mins[0], maxs[1] - mins[1], maxs[2] - mins[2]});
      Link(entity);
    });

    set(QcBuiltinNumber::SetModel, [this](QcMachine &machine)
    {
      SetModel(machine.GetParameterInteger(0), machine.GetParameterInteger(1), machine.GetParameterString(1));
    });

    set(QcBuiltinNumber::MakeStatic, [this](QcMachine &machine)
    {
      MakeStatic(machine.GetParameterInteger(0));
    });

    // Turns the entity that runs towards where it wants to look, by as much
    // as it turns in a step.
    set(QcBuiltinNumber::ChangeYaw, [this](QcMachine &machine)
    {
      const QcFields &fields = _level->fields;
      const std::int32_t self = _level->globals.self.Get(machine);

      Vector angles = fields.angles.Get(machine, self);
      const float current = wrap_angle(angles[1]);
      const float speed = fields.yaw_speed.Get(machine, self);

      float move = fields.ideal_yaw.Get(machine, self) - current;
      if (move > 180.0f) { move -= 360.0f; } else if (move < -180.0f) { move += 360.0f; }
      move = std::clamp(move, -speed, speed);

      angles[1] = wrap_angle(current + move);
      fields.angles.Set(machine, self, angles);
    });

    // What needs the walls of the level is answered with nothing in the
    // way, until the level is collided with as the original does.

    // a line through the level reaches its end
    set(QcBuiltinNumber::TraceLine, [this](QcMachine &machine)
    {
      const QcGlobals &globals = _level->globals;
      globals.trace_allsolid.Set(machine, 0.0f);
      globals.trace_startsolid.Set(machine, 0.0f);
      globals.trace_fraction.Set(machine, 1.0f);
      globals.trace_inopen.Set(machine, 1.0f);
      globals.trace_inwater.Set(machine, 0.0f);
      globals.trace_endpos.Set(machine, machine.GetParameterVector(1));
      globals.trace_plane_normal.Set(machine, {0.0f, 0.0f, 0.0f});
      globals.trace_plane_dist.Set(machine, 0.0f);
      globals.trace_ent.Set(machine, 0);
    });

    // An item or a monster is on the floor where the level put it. Saying
    // no has the game code remove it as fallen out of the level.
    set(QcBuiltinNumber::DropToFloor, [](QcMachine &machine) { machine.SetReturnFloat(1.0f); });

    // Nothing walks yet. A step of no length is how the game code asks
    // whether a monster stands in a wall, and it does not.
    set(QcBuiltinNumber::WalkMove, [](QcMachine &machine)
    {
      machine.SetReturnFloat(machine.GetParameterFloat(1) == 0.0f ? 1.0f : 0.0f);
    });
    set(QcBuiltinNumber::MoveToGoal, [](QcMachine &machine) {});
    set(QcBuiltinNumber::CheckBottom, [](QcMachine &machine) { machine.SetReturnFloat(1.0f); });

    // every place is in the open: -1 is what the game code has for empty
    set(QcBuiltinNumber::PointContents, [](QcMachine &machine) { machine.SetReturnFloat(-1.0f); });

    // a shot goes where the player looks
    set(QcBuiltinNumber::Aim, [this](QcMachine &machine)
    {
      machine.SetReturnVector(_level->globals.v_forward.Get(machine));
    });

    // no monster sees the player yet, and nothing is near anything
    set(QcBuiltinNumber::CheckClient, [](QcMachine &machine) { machine.SetReturnInteger(0); });
    set(QcBuiltinNumber::FindRadius, [](QcMachine &machine) { machine.SetReturnInteger(0); });

    // nothing is heard yet, and no sparks fly
    set(QcBuiltinNumber::Sound, [](QcMachine &machine) {});
    set(QcBuiltinNumber::AmbientSound, [](QcMachine &machine) {});
    set(QcBuiltinNumber::Particle, [](QcMachine &machine) {});
  }

  bool GameCode::MovePusher(const std::int32_t entity, const Vector &origin, const Vector &angles, const float dt)
  {
    // Nothing holds a door back yet: what moves in the engine pushes what
    // is in its way. The box of where it is goes with it.
    QcMachine &machine = _level->machine;
    const QcFields &fields = _level->fields;
    const Vector mins = fields.mins.Get(machine, entity);
    const Vector maxs = fields.maxs.Get(machine, entity);
    fields.absmin.Set(machine, entity, {origin[0] + mins[0] - 1.0f, origin[1] + mins[1] - 1.0f, origin[2] + mins[2] - 1.0f});
    fields.absmax.Set(machine, entity, {origin[0] + maxs[0] + 1.0f, origin[1] + maxs[1] + 1.0f, origin[2] + maxs[2] + 1.0f});
    return true;
  }

  void GameCode::PrintToAll(const std::string_view text)
  {
    // The game code prints a line in pieces, and ends it with a line end.
    _line += text;
    if (!_line.ends_with('\n')) { return; }

    if (const std::string plain = to_plain(_line); !plain.empty()) { _world->Info(plain); }
    _line.clear();
  }

  void GameCode::PrintToClient(const std::int32_t client, const std::string_view text)
  {
    PrintToAll(text);
  }

  void GameCode::PrintToCenter(const std::int32_t client, const std::string_view text)
  {
    // what is shown in the middle of the screen is whole, and ends with none
    if (const std::string plain = to_plain(text); !plain.empty()) { _world->Info(plain); }
  }

  void GameCode::Error(const std::int32_t self, const std::string_view text)
  {
    _world->Error("The game code stopped with an error: " + to_plain(text));
  }

  void GameCode::ObjectError(const std::int32_t entity, const std::string_view text)
  {
    _world->Warn("The game code removed entity " + std::to_string(entity) + ": " + to_plain(text));
  }

  void GameCode::EntityRemoved(const std::int32_t entity)
  {
    if (entity > 0 && static_cast<std::size_t>(entity) < _shown.size()) { Hide(_shown[entity]); }
  }

  void GameCode::ChangeLevel(const std::string_view level)
  {
    _world->Info("The game code asks for the level " + std::string(level) + ", which is not gone to yet");
  }

  void GameCode::Hide(Shown &shown)
  {
    if (shown.entity != 0)
    {
      // A model of the level that the game code gave up is gone for the rest
      // of the level, as an entity made here is.
      if (shown.is_alias) { _models->Forget(shown.entity); }
      _world->DestroyEntity(shown.entity);
    }
    shown = {};
  }

  void GameCode::Show(const std::int32_t entity)
  {
    QcMachine &machine = _level->machine;
    const QcFields &fields = _level->fields;

    if (static_cast<std::size_t>(entity) >= _shown.size()) { _shown.resize(static_cast<std::size_t>(entity) + 1); }
    Shown &shown = _shown[entity];

    // An entity shows the model it names for as long as it has a number of
    // a model. The player is the eyes of the game and sees no body.
    std::string_view model;
    if (!machine.IsEntityFree(entity) && entity != player_entity && fields.modelindex.Get(machine, entity) != 0.0f)
    {
      model = fields.model.GetText(machine, entity);
    }

    if (model != shown.model)
    {
      // a model of the level that is not named for a while is kept, out of
      // sight, since it cannot be made again
      if (shown.is_part && model.empty())
      {
        _world->SetVector3(shown.entity, _position_field, {0.0f, -10000.0f, 0.0f});
        shown.model.clear();
        shown.is_placed = false;
        return;
      }
      if (!shown.is_part) { Hide(shown); }
      shown.model = std::string(model);
      shown.is_placed = false;

      if (const std::size_t part = read_part_number(model); part > 0)
      {
        shown.entity = _view->FindPart(part);
        shown.is_part = true;
      } else if (model.ends_with(".mdl"))
      {
        shown.entity = _world->CreateEntity("entity " + std::to_string(entity));
        shown.is_alias = true;
      } else if (model.ends_with(".bsp"))
      {
        shown.entity = _world->CreateEntity("entity " + std::to_string(entity));
        _world->AddComponent(shown.entity, "Transform");
        _view->ShowItem(*_world, *_data, shown.model, shown.entity);
      }
      // a sprite, `.spr`, is not shown yet
    }
    if (shown.entity == 0) { return; }

    if (shown.is_alias)
    {
      const auto frame = static_cast<std::int32_t>(fields.frame.Get(machine, entity));
      const auto skin = static_cast<std::int32_t>(fields.skin.Get(machine, entity));
      if (!_models->Show(*_world, *_data, shown.entity, shown.model, frame, skin))
      {
        Hide(shown);
        // kept by its name, so that it is not tried again in every step
        shown.model = std::string(model);
        return;
      }
    }

    const Vector origin = fields.origin.Get(machine, entity);
    Vector angles = fields.angles.Get(machine, entity);
    if (shown.is_alias && (_models->GetFlags(shown.entity) & turns_flag) != 0)
    {
      angles = {0.0f, wrap_angle(static_cast<float>(_level->running.GetTime() * turn_speed)), 0.0f};
    }

    if (!shown.is_placed || origin != shown.origin)
    {
      const BspVector place = QuakeSpace::ToEnginePosition({origin[0], origin[1], origin[2]});
      _world->SetVector3(shown.entity, _position_field, {place.x, place.y, place.z});
    }
    // A model of the level does not turn in the original game. The angles of
    // the game are pitch, yaw, and roll: around what points left, up, and
    // forward, which the engine has as its z, y, and x.
    if (!shown.is_part && (!shown.is_placed || angles != shown.angles))
    {
      _world->SetVector3(shown.entity, _rotation_field, {angles[2], angles[1], angles[0]});
    }
    shown.origin = origin;
    shown.angles = angles;
    shown.is_placed = true;
  }

  void GameCode::ReadPlayer(const float dt)
  {
    if (_player == 0)
    {
      _player = _world->FindEntity("player");
      if (_player == 0) { return; }
    }
    // until the player stands where the game code has it, there is nothing
    // to tell
    if (!_player_placed) { return; }

    QcMachine &machine = _level->machine;
    const QcFields &fields = _level->fields;

    const neon::extension::Vector3 position = _world->GetVector3(_player, _position_field);
    const BspVector place = QuakeSpace::ToGamePosition({position.x, position.y, position.z});
    const Vector origin = {place.x, place.y, place.z - middle_height};

    if (dt > 0.0f)
    {
      fields.velocity.Set(machine, player_entity, {
        (origin[0] - _player_origin[0]) / dt,
        (origin[1] - _player_origin[1]) / dt,
        (origin[2] - _player_origin[2]) / dt,
      });
    }
    fields.origin.Set(machine, player_entity, origin);
    Link(player_entity);

    // the yaw of the engine is 0 towards the game's north, see QuakeSpace
    const float yaw = wrap_angle(_world->GetVector3(_player, _rotation_field).y + 90.0f);
    fields.angles.Set(machine, player_entity, {0.0f, yaw, 0.0f});
    fields.v_angle.Set(machine, player_entity, {0.0f, yaw, 0.0f});

    _player_origin = origin;
  }

  void GameCode::PlacePlayer()
  {
    if (_player == 0) { return; }

    QcMachine &machine = _level->machine;
    const QcFields &fields = _level->fields;

    // The game code moved the player when it is not where it was said to
    // be: at the start of a level, and through a teleporter.
    const Vector origin = fields.origin.Get(machine, player_entity);
    if (_player_placed && origin == _player_origin) { return; }

    const BspVector place = QuakeSpace::ToEnginePosition({origin[0], origin[1], origin[2] + middle_height});
    _world->SetVector3(_player, _position_field, {place.x, place.y, place.z});

    // and it says so when the player is to look another way
    if (!_player_placed || fields.fixangle.Get(machine, player_entity) != 0.0f)
    {
      const float yaw = QuakeSpace::ToEngineYaw(fields.angles.Get(machine, player_entity)[1]);
      _world->SetVector3(_player, _rotation_field, {0.0f, yaw, 0.0f});
      fields.fixangle.Set(machine, player_entity, 0.0f);
    }

    _player_origin = origin;
    _player_placed = true;
  }

  void GameCode::TouchAsPlayer()
  {
    if (!_player_placed) { return; }

    QcMachine &machine = _level->machine;
    const QcFields &fields = _level->fields;

    const Vector origin = fields.origin.Get(machine, player_entity);
    const Vector mins = fields.mins.Get(machine, player_entity);
    const Vector maxs = fields.maxs.Get(machine, player_entity);

    // those there are now: what a touch makes is not touched in the same step
    const std::int32_t count = machine.GetEntityCount();
    for (std::int32_t entity = player_entity + 1; entity < count; entity++)
    {
      if (machine.IsEntityFree(entity)) { continue; }

      const auto solid = static_cast<QcSolid>(static_cast<int>(fields.solid.Get(machine, entity)));
      const std::int32_t touch = fields.touch.Get(machine, entity);
      if (solid == QcSolid::Not || touch == 0) { continue; }

      // A trigger is touched from inside it. What stops the player is
      // touched by standing at it, which is a little off it for the body of
      // the engine.
      const float reach = solid == QcSolid::Trigger ? 0.0f : touch_reach;
      const Vector other_origin = fields.origin.Get(machine, entity);
      const Vector other_mins = fields.mins.Get(machine, entity);
      const Vector other_maxs = fields.maxs.Get(machine, entity);

      bool is_touching = true;
      for (std::size_t axis = 0; axis < 3; axis++)
      {
        if (origin[axis] + mins[axis] > other_origin[axis] + other_maxs[axis] + reach ||
            origin[axis] + maxs[axis] < other_origin[axis] + other_mins[axis] - reach)
        {
          is_touching = false;
          break;
        }
      }
      if (is_touching) { _level->running.RunFunction(touch, entity, player_entity); }
    }
  }

  void GameCode::SayFailures()
  {
    const std::vector<LevelFailure> &failures = _level->running.GetFailures();
    for (; _failures_said < failures.size(); _failures_said++)
    {
      const LevelFailure &failure = failures[_failures_said];
      // what the game code stopped itself for was said when it did
      if (failure.error.message.starts_with(QcCoreBuiltins::error_start)) { continue; }

      _world->Warn("The game code failed in " + failure.function + " of entity " + std::to_string(failure.entity) +
        ", " + failure.classname + ": " + failure.error.message);
    }
  }

  bool GameCode::Start(
    const World &world,
    const GameData &data,
    LevelView &view,
    ModelView &models,
    const std::string &map,
    std::string &error)
  {
    _world = &world;
    _data = &data;
    _view = &view;
    _models = &models;
    _position_field = world.FindField("Transform", "position");
    _rotation_field = world.FindField("Transform", "rotation");

    _level.reset();
    _shown.clear();
    _player = 0;
    _player_placed = false;
    _failures_said = 0;

    const std::span<const std::uint8_t> code = data.Find("progs.dat");
    if (code.empty())
    {
      error = "the data of the game holds no progs.dat";
      return false;
    }

    Progs progs;
    if (std::string problem; !progs.Read(code, problem))
    {
      error = "progs.dat cannot be read: " + problem;
      return false;
    }

    auto level = std::make_unique<Level>(std::move(progs), *this, *this);
    if (std::string problem; !level->file.Read(data.Find(map), problem))
    {
      error = map + " cannot be read: " + problem;
      return false;
    }

    EntityText text;
    if (std::string problem; !text.Read(level->file.entities, problem))
    {
      error = "the entities of " + map + " cannot be read: " + problem;
      return false;
    }

    if (!level->globals.HasEssentials())
    {
      error = "progs.dat is not the game code of this game";
      return false;
    }

    _level = std::move(level);
    _level->builtins.Register(_level->machine);
    RegisterBuiltins();

    // The models the game code finds announced: the level, which is model 1,
    // and each of its models after it.
    QcPrecacheList &announced = _level->builtins.GetModels();
    announced.Add(map);
    for (std::size_t part = 1; part < _level->file.models.size(); part++) { announced.Add("*" + std::to_string(part)); }

    // maps/start.bsp is known to the game code as start
    std::string name = map;
    if (const std::size_t slash = name.rfind('/'); slash != std::string::npos) { name.erase(0, slash + 1); }
    if (name.ends_with(".bsp")) { name.erase(name.size() - 4); }

    LevelSpawningSettings settings;
    settings.map_name = name;
    settings.model_name = map;
    settings.skill = static_cast<int>(_level->builtins.GetVariables().GetFloat("skill"));

    LevelSpawning spawning(_level->machine);
    const LevelSpawningReport report = spawning.Spawn(text.entities, settings);
    for (const LevelFailure &failure : report.failures)
    {
      if (failure.error.message.starts_with(QcCoreBuiltins::error_start)) { continue; }
      world.Warn("The game code failed making " + failure.classname + ": " + failure.error.message);
    }

    _level->running.ConnectClient(player_entity, "player");

    // The original lets two steps pass before a player sees the level, in
    // which what was made settles: doors find their other halves, items
    // come to lie.
    _level->running.Advance(0.1f);
    _level->running.Advance(0.1f);

    // A model of the level no entity of the game code names is one that was
    // left out, for the skill that is played, and is not there.
    std::vector<bool> is_named(_level->file.models.size(), false);
    for (std::int32_t entity = 1; entity < _level->machine.GetEntityCount(); entity++)
    {
      if (_level->machine.IsEntityFree(entity)) { continue; }

      const std::size_t part = read_part_number(_level->fields.model.GetText(_level->machine, entity));
      if (part < is_named.size()) { is_named[part] = true; }
    }
    for (std::size_t part = 1; part < is_named.size(); part++)
    {
      if (const Entity shown = view.FindPart(part); !is_named[part] && shown != 0) { world.DestroyEntity(shown); }
    }

    for (std::int32_t entity = 1; entity < _level->machine.GetEntityCount(); entity++) { Show(entity); }
    SayFailures();

    world.Info("The game code runs " + name + ": " + std::to_string(report.spawned) + " entities made, " +
      std::to_string(report.left_out) + " left out, " + std::to_string(report.without_function) + " unknown to it, " +
      std::to_string(static_cast<int>(_level->globals.total_monsters.Get(_level->machine))) + " monsters");
    return true;
  }

  bool GameCode::IsRunning() const
  {
    return _level != nullptr;
  }

  void GameCode::Advance(const World &world, const float dt)
  {
    if (_level == nullptr) { return; }
    _world = &world;

    ReadPlayer(dt);

    _level->running.RunClientThink(player_entity, ClientThink::Before);
    _level->running.Advance(dt);
    TouchAsPlayer();
    _level->running.RunClientThink(player_entity, ClientThink::After);

    PlacePlayer();
    for (std::int32_t entity = 1; entity < _level->machine.GetEntityCount(); entity++) { Show(entity); }
    SayFailures();
  }
} // quake
