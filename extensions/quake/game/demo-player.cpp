#include "demo-player.hpp"

#include <algorithm>
#include <cmath>
#include <format>

#include "demo-entity-bit.hpp"

namespace quake
{
  // The numbers and the arithmetic of putting things between two moments,
  // which need nothing of the object.
  namespace
  {
    /// The longest two blocks are taken to be apart, in seconds. A server
    /// sends one about this often, and over this time an entity that
    /// moves in steps is moved from one step to the next.
    constexpr float longest_step = 0.1f;

    /// How far on an axis an entity may move between two updates. What is
    /// farther is taken for a teleporter, and nothing is put between.
    constexpr float longest_move = 100.0f;

    /// A place between two, by a share from 0 to 1.
    LevelVector BetweenPlaces(const LevelVector &from, const LevelVector &to, const float share)
    {
      return Sum(from, Scaled(Difference(to, from), share));
    }

    /// Angles between two, each the short way around.
    LevelVector BetweenAngles(const LevelVector &from, const LevelVector &to, const float share)
    {
      LevelVector angles;
      for (std::size_t axis = 0; axis < 3; axis++)
      {
        float turn = to[axis] - from[axis];
        if (turn > 180.0f) { turn -= 360.0f; }
        else if (turn < -180.0f) { turn += 360.0f; }
        angles[axis] = from[axis] + share * turn;
      }
      return angles;
    }

    /// Whether an entity went too far between two updates to have moved.
    bool IsTeleport(const LevelVector &from, const LevelVector &to)
    {
      for (std::size_t axis = 0; axis < 3; axis++)
      {
        if (std::abs(to[axis] - from[axis]) > longest_move) { return true; }
      }
      return false;
    }

    std::string_view NameAt(const std::vector<std::string> &names, const std::int32_t index)
    {
      if (index < 0 || static_cast<std::size_t>(index) >= names.size()) { return {}; }

      return names[static_cast<std::size_t>(index)];
    }
  }

  DemoPlayer::DemoPlayer(DemoListener &listener)
    : _listener(listener)
  {
  }

  bool DemoPlayer::Open(const std::span<const std::uint8_t> bytes, std::string &problem)
  {
    Close();

    _bytes.assign(bytes.begin(), bytes.end());
    if (!_file.Read(_bytes, problem))
    {
      Close();
      return false;
    }
    _is_open = true;
    return true;
  }

  void DemoPlayer::Close()
  {
    ClearLevel();
    _file = {};
    _bytes.clear();
    _reader.Reset();
    _next_block = 0;
    _is_open = false;
    _is_over = false;
    _problem.clear();
    _block_view_angles = {};
    _view_angles = {};
  }

  void DemoPlayer::ClearLevel()
  {
    _time = 0.0f;
    _message_times = {};
    _signon = 0;
    _has_level = false;
    _info = {};
    _slots.clear();
    _entities.clear();
    _static_entities.clear();
    _static_sounds.clear();
    _view_entity = 0;
    _client_data = {};
    _stats = {};
    for (std::string &style : _light_styles) { style.clear(); }
    _scores.clear();
    _is_paused = false;
    _intermission = DemoIntermission::None;
    _completed_time = 0.0f;
  }

  DemoPlayer::Slot &DemoPlayer::SlotOf(const std::int32_t entity)
  {
    // the reader hands out numbers of two bytes at most
    const auto index = static_cast<std::size_t>(std::clamp(entity, 0, 65535));
    if (index >= _slots.size()) { _slots.resize(index + 1); }
    return _slots[index];
  }

  void DemoPlayer::Advance(const float seconds)
  {
    if (!_is_open || _is_over) { return; }

    if (seconds > 0.0f) { _time += seconds; }

    const std::span<const DemoBlock> blocks = _file.GetBlocks();
    while (true)
    {
      // a client that has joined waits until the clock is past the last block
      if (_signon == signed_on && _time <= _message_times[0]) { break; }

      if (_next_block >= blocks.size())
      {
        _is_over = true;
        break;
      }
      const std::size_t index = _next_block++;
      const DemoBlock &block = blocks[index];
      _block_view_angles[1] = _block_view_angles[0];
      _block_view_angles[0] = block.view_angles;

      std::string problem;
      if (!_reader.Read(block.message, *this, problem))
      {
        _problem = std::format("Block {}: {}", index, problem);
        _is_over = true;
      }
      if (_is_over) { break; }
    }
    Relink();
  }

  float DemoPlayer::LerpPoint()
  {
    float span = _message_times[0] - _message_times[1];
    if (!(span > 0.0f))
    {
      _time = _message_times[0];
      return 1.0f;
    }

    // a block was lost, or these are the first two of a level
    if (span > longest_step)
    {
      _message_times[1] = _message_times[0] - longest_step;
      span = longest_step;
    }

    float share = (_time - _message_times[1]) / span;
    if (share < 0.0f)
    {
      if (share < -0.01f) { _time = _message_times[1]; }
      share = 0.0f;
    }
    else if (share > 1.0f)
    {
      if (share > 1.01f) { _time = _message_times[0]; }
      share = 1.0f;
    }
    return share;
  }

  void DemoPlayer::Relink()
  {
    const float share = LerpPoint();
    _view_angles = BetweenAngles(_block_view_angles[1], _block_view_angles[0], share);

    // the level itself is entity 0 and is not among them
    _entities.clear();
    for (std::size_t number = 1; number < _slots.size(); number++)
    {
      Slot &slot = _slots[number];
      if (slot.state.model == 0) { continue; }

      // the last packet said nothing of it: it is gone
      if (!slot.was_updated || slot.message_time != _message_times[0])
      {
        slot.state.model = 0;
        continue;
      }

      bool is_placed_anew = slot.is_new;
      if (slot.is_new)
      {
        slot.state.origin = slot.origins[0];
        slot.state.angles = slot.angles[0];
      }
      else if (slot.moves_in_steps)
      {
        const float step_share = std::clamp((_time - slot.step_time) / longest_step, 0.0f, 1.0f);
        slot.state.origin = BetweenPlaces(slot.step_from_origin, slot.step_to_origin, step_share);
        slot.state.angles = BetweenAngles(slot.step_from_angles, slot.step_to_angles, step_share);
      }
      else
      {
        const bool is_teleport = IsTeleport(slot.origins[1], slot.origins[0]);
        const float entity_share = is_teleport ? 1.0f : share;
        is_placed_anew = is_teleport;
        slot.state.origin = BetweenPlaces(slot.origins[1], slot.origins[0], entity_share);
        slot.state.angles = BetweenAngles(slot.angles[1], slot.angles[0], entity_share);
      }
      slot.is_new = false;

      _entities.push_back({
        .number = static_cast<std::int32_t>(number),
        .state = slot.state,
        .moves_in_steps = slot.moves_in_steps,
        .is_placed_anew = is_placed_anew,
        .frame_finish_time = slot.frame_finish_time,
      });
    }
  }

  void DemoPlayer::StartIntermission(const DemoIntermission intermission)
  {
    _intermission = intermission;
    _completed_time = _time;
  }

  void DemoPlayer::HandleServerInfo(const DemoServerInfo &info)
  {
    ClearLevel();
    _info = info;
    _has_level = true;
    _scores.resize(static_cast<std::size_t>(std::max(info.max_clients, 0)));
    _listener.LevelBegan(_info);
  }

  void DemoPlayer::HandleTime(const float seconds)
  {
    _message_times[1] = _message_times[0];
    _message_times[0] = seconds;
  }

  void DemoPlayer::HandleSignon(const std::int32_t stage)
  {
    _signon = stage;
  }

  void DemoPlayer::HandlePause(const bool is_paused)
  {
    _is_paused = is_paused;
  }

  void DemoPlayer::HandleDisconnect()
  {
    _is_over = true;
  }

  void DemoPlayer::HandleBaseline(const std::int32_t entity, const DemoEntityState &state)
  {
    SlotOf(entity).baseline = state;
  }

  void DemoPlayer::HandleEntityUpdate(const DemoEntityUpdate &update)
  {
    // the first update is the last step of joining the level
    if (_signon == signed_on - 1) { _signon = signed_on; }

    Slot &slot = SlotOf(update.entity);
    const DemoEntityState &baseline = slot.baseline;

    // without an update in the block before there is nothing to move it from
    bool is_new = !slot.was_updated || slot.message_time != _message_times[1];
    slot.message_time = _message_times[0];
    slot.was_updated = true;

    // what was not sent is as the baseline has it
    DemoEntityState &state = slot.state;
    state.model = update.Has(DemoEntityBit::Model) ? update.model : baseline.model;
    state.frame = update.Has(DemoEntityBit::Frame) ? update.frame : baseline.frame;
    state.colormap = update.Has(DemoEntityBit::Colormap) ? update.colormap : baseline.colormap;
    state.skin = update.Has(DemoEntityBit::Skin) ? update.skin : baseline.skin;
    state.effects = update.Has(DemoEntityBit::Effects) ? update.effects : baseline.effects;
    state.alpha = baseline.alpha;
    state.scale = baseline.scale;
    slot.frame_finish_time = 0.0f;
    if (_reader.GetProtocol().HasExtensions())
    {
      if (update.Has(DemoEntityBit::Alpha)) { state.alpha = update.alpha; }
      if (update.Has(DemoEntityBit::Scale)) { state.scale = update.scale; }
      if (update.Has(DemoEntityBit::Frame2)) { state.frame = (state.frame & 0xff) | update.frame_high << 8; }
      if (update.Has(DemoEntityBit::Model2)) { state.model = (state.model & 0xff) | update.model_high << 8; }
      if (update.Has(DemoEntityBit::LerpFinish))
      {
        slot.frame_finish_time = slot.message_time + update.lerp_finish;
      }
    }
    slot.moves_in_steps = update.Has(DemoEntityBit::Step);

    slot.origins[1] = slot.origins[0];
    slot.angles[1] = slot.angles[0];
    constexpr DemoEntityBit origin_bits[] = {DemoEntityBit::Origin1, DemoEntityBit::Origin2, DemoEntityBit::Origin3};
    constexpr DemoEntityBit angle_bits[] = {DemoEntityBit::Angle1, DemoEntityBit::Angle2, DemoEntityBit::Angle3};
    for (std::size_t axis = 0; axis < 3; axis++)
    {
      slot.origins[0][axis] = update.Has(origin_bits[axis]) ? update.origin[axis] : baseline.origin[axis];
      slot.angles[0][axis] = update.Has(angle_bits[axis]) ? update.angles[axis] : baseline.angles[axis];
    }

    if (state.model == 0) { is_new = true; }
    if (is_new)
    {
      slot.origins[1] = slot.origins[0];
      slot.angles[1] = slot.angles[0];
      slot.is_new = true;
    }

    // a step is from where the step before ended to where this one does
    if (is_new || IsTeleport(slot.step_to_origin, slot.origins[0]))
    {
      slot.step_from_origin = slot.origins[0];
      slot.step_from_angles = slot.angles[0];
      slot.step_to_origin = slot.origins[0];
      slot.step_to_angles = slot.angles[0];
      slot.step_time = _time;
    }
    else if (slot.step_to_origin != slot.origins[0] || slot.step_to_angles != slot.angles[0])
    {
      slot.step_from_origin = slot.step_to_origin;
      slot.step_from_angles = slot.step_to_angles;
      slot.step_to_origin = slot.origins[0];
      slot.step_to_angles = slot.angles[0];
      slot.step_time = _time;
    }
  }

  void DemoPlayer::HandleStaticEntity(const DemoEntityState &state)
  {
    _static_entities.push_back(state);
  }

  void DemoPlayer::HandleViewEntity(const std::int32_t entity)
  {
    _view_entity = entity;
  }

  void DemoPlayer::HandleClientData(const DemoClientData &data)
  {
    _client_data = data;

    // what the status bar shows of it is kept with the other numbers
    _stats[stat_weapon_frame] = data.weapon_frame;
    _stats[stat_armor] = data.armor;
    _stats[stat_weapon_model] = data.weapon_model;
    _stats[stat_active_weapon] = static_cast<std::int32_t>(data.active_weapon);
    _stats[stat_health] = data.health;
    _stats[stat_ammo] = data.ammo;
    _stats[stat_shells] = data.shells;
    _stats[stat_nails] = data.nails;
    _stats[stat_rockets] = data.rockets;
    _stats[stat_cells] = data.cells;
  }

  void DemoPlayer::HandleStat(const std::int32_t stat, const std::int32_t value)
  {
    if (stat < 0 || static_cast<std::size_t>(stat) >= stat_count) { return; }

    _stats[static_cast<std::size_t>(stat)] = value;
  }

  void DemoPlayer::HandleDamage(const DemoDamage &damage)
  {
    _listener.DamageTaken(damage);
  }

  void DemoPlayer::HandleBonusFlash()
  {
    _listener.BonusFlashed();
  }

  void DemoPlayer::HandleSound(const DemoSound &sound)
  {
    _listener.SoundStarted(sound);
  }

  void DemoPlayer::HandleStopSound(const std::int32_t entity, const std::int32_t channel)
  {
    _listener.SoundStopped(entity, channel);
  }

  void DemoPlayer::HandleStaticSound(const DemoStaticSound &sound)
  {
    _static_sounds.push_back(sound);
  }

  void DemoPlayer::HandleMusicTrack(const std::int32_t track, const std::int32_t loop_track)
  {
    // the one who recorded may have forced a track, which is a byte too
    const std::int32_t forced = _file.GetForcedTrack();
    _listener.MusicTrackSet(forced == DemoFile::no_forced_track ? track : forced & 255, loop_track);
  }

  void DemoPlayer::HandlePointEffect(const TempEntityPoint &effect)
  {
    _listener.PointEffect(effect);
  }

  void DemoPlayer::HandleColoredExplosion(const TempEntityExplosion &explosion)
  {
    _listener.ColoredExplosion(explosion);
  }

  void DemoPlayer::HandleBeamEffect(const TempEntityBeam &beam)
  {
    _listener.BeamEffect(beam);
  }

  void DemoPlayer::HandleParticles(const DemoParticles &particles)
  {
    _listener.ParticlesBurst(particles);
  }

  void DemoPlayer::HandleLightStyle(const std::int32_t style, const std::string_view text)
  {
    if (style < 0 || static_cast<std::size_t>(style) >= light_style_count) { return; }

    _light_styles[static_cast<std::size_t>(style)] = text;
  }

  void DemoPlayer::HandleSkybox(const std::string_view name)
  {
    _listener.SkyboxSet(name);
  }

  void DemoPlayer::HandleFog(const DemoFog &fog)
  {
    _listener.FogSet(fog);
  }

  void DemoPlayer::HandlePrint(const std::string_view text)
  {
    _listener.TextPrinted(text);
  }

  void DemoPlayer::HandleCenterPrint(const std::string_view text)
  {
    _listener.CenterTextPrinted(text);
  }

  void DemoPlayer::HandleStuffText(const std::string_view text)
  {
    _listener.CommandGiven(text);
  }

  void DemoPlayer::HandlePlayerName(const std::int32_t player, const std::string_view name)
  {
    if (player < 0 || static_cast<std::size_t>(player) >= _scores.size()) { return; }

    _scores[static_cast<std::size_t>(player)].name = name;
  }

  void DemoPlayer::HandlePlayerFrags(const std::int32_t player, const std::int32_t frags)
  {
    if (player < 0 || static_cast<std::size_t>(player) >= _scores.size()) { return; }

    _scores[static_cast<std::size_t>(player)].frags = frags;
  }

  void DemoPlayer::HandlePlayerColors(const std::int32_t player, const std::int32_t colors)
  {
    if (player < 0 || static_cast<std::size_t>(player) >= _scores.size()) { return; }

    _scores[static_cast<std::size_t>(player)].colors = colors;
  }

  void DemoPlayer::HandleKilledMonster()
  {
    _stats[stat_monsters]++;
  }

  void DemoPlayer::HandleFoundSecret()
  {
    _stats[stat_secrets]++;
  }

  void DemoPlayer::HandleIntermission()
  {
    StartIntermission(DemoIntermission::Tally);
    _listener.IntermissionStarted();
  }

  void DemoPlayer::HandleFinale(const std::string_view text)
  {
    StartIntermission(DemoIntermission::Finale);
    _listener.FinaleStarted(text);
  }

  void DemoPlayer::HandleCutscene(const std::string_view text)
  {
    StartIntermission(DemoIntermission::Cutscene);
    _listener.CutsceneStarted(text);
  }

  void DemoPlayer::HandleSellScreen()
  {
    _listener.SellScreenShown();
  }

  bool DemoPlayer::IsOpen() const
  {
    return _is_open;
  }

  bool DemoPlayer::IsOver() const
  {
    return _is_over;
  }

  bool DemoPlayer::HasFailed() const
  {
    return !_problem.empty();
  }

  const std::string &DemoPlayer::GetProblem() const
  {
    return _problem;
  }

  float DemoPlayer::GetTime() const
  {
    return _time;
  }

  bool DemoPlayer::IsSignedOn() const
  {
    return _signon == signed_on;
  }

  bool DemoPlayer::IsPaused() const
  {
    return _is_paused;
  }

  DemoIntermission DemoPlayer::GetIntermission() const
  {
    return _intermission;
  }

  float DemoPlayer::GetCompletedTime() const
  {
    return _completed_time;
  }

  bool DemoPlayer::HasLevel() const
  {
    return _has_level;
  }

  const DemoServerInfo &DemoPlayer::GetServerInfo() const
  {
    return _info;
  }

  const std::string &DemoPlayer::GetLevelName() const
  {
    return _info.level_name;
  }

  std::string_view DemoPlayer::GetMapModelName() const
  {
    return NameAt(_info.model_names, 1);
  }

  std::span<const std::string> DemoPlayer::GetModelNames() const
  {
    return _info.model_names;
  }

  std::span<const std::string> DemoPlayer::GetSoundNames() const
  {
    return _info.sound_names;
  }

  std::string_view DemoPlayer::GetModelName(const std::int32_t index) const
  {
    return NameAt(_info.model_names, index);
  }

  std::string_view DemoPlayer::GetSoundName(const std::int32_t index) const
  {
    return NameAt(_info.sound_names, index);
  }

  std::string_view DemoPlayer::GetLightStyle(const std::size_t style) const
  {
    if (style >= light_style_count) { return {}; }

    return _light_styles[style];
  }

  std::span<const DemoEntity> DemoPlayer::GetEntities() const
  {
    return _entities;
  }

  const DemoEntity *DemoPlayer::FindEntity(const std::int32_t number) const
  {
    // they are in the order of their numbers
    const auto found = std::lower_bound(
      _entities.begin(), _entities.end(), number,
      [](const DemoEntity &entity, const std::int32_t wanted) { return entity.number < wanted; });
    return found != _entities.end() && found->number == number ? &*found : nullptr;
  }

  std::span<const DemoEntityState> DemoPlayer::GetStaticEntities() const
  {
    return _static_entities;
  }

  std::span<const DemoStaticSound> DemoPlayer::GetStaticSounds() const
  {
    return _static_sounds;
  }

  std::int32_t DemoPlayer::GetViewEntity() const
  {
    return _view_entity;
  }

  const LevelVector &DemoPlayer::GetViewAngles() const
  {
    return _view_angles;
  }

  const DemoClientData &DemoPlayer::GetClientData() const
  {
    return _client_data;
  }

  std::int32_t DemoPlayer::GetStat(const std::size_t stat) const
  {
    return stat < stat_count ? _stats[stat] : 0;
  }

  PlayerStats DemoPlayer::GetStats() const
  {
    PlayerStats stats;
    stats.health = _stats[stat_health];
    stats.armor = _stats[stat_armor];
    stats.ammo = _stats[stat_ammo];
    stats.shells = _stats[stat_shells];
    stats.nails = _stats[stat_nails];
    stats.rockets = _stats[stat_rockets];
    stats.cells = _stats[stat_cells];
    stats.weapon = static_cast<std::uint32_t>(_stats[stat_active_weapon]);
    stats.items = _client_data.items;
    stats.killed_monsters = _stats[stat_monsters];
    stats.total_monsters = _stats[stat_total_monsters];
    stats.found_secrets = _stats[stat_secrets];
    stats.total_secrets = _stats[stat_total_secrets];
    stats.level_name = _info.level_name;
    stats.time = _time;

    // the name of the file of the level without its folder and its ending
    std::string_view map_name = GetMapModelName();
    if (const std::size_t slash = map_name.rfind('/'); slash != std::string_view::npos)
    {
      map_name.remove_prefix(slash + 1);
    }
    if (const std::size_t dot = map_name.rfind('.'); dot != std::string_view::npos)
    {
      map_name = map_name.substr(0, dot);
    }
    stats.map_name = map_name;
    return stats;
  }

  std::int32_t DemoPlayer::GetWeaponModel() const
  {
    return _stats[stat_weapon_model];
  }

  std::int32_t DemoPlayer::GetWeaponFrame() const
  {
    return _stats[stat_weapon_frame];
  }

  std::span<const DemoScore> DemoPlayer::GetScores() const
  {
    return _scores;
  }
} // quake
