#include "demo-message-reader.hpp"

#include <algorithm>
#include <format>

#include "demo-entity-bit.hpp"
#include "demo-message-kind.hpp"
#include "temp-entity-kind.hpp"

namespace quake
{
  // The bits of the messages that have some, which need nothing of the
  // object.
  namespace
  {
    // Of a sound: what follows the byte that has them.
    constexpr std::uint32_t sound_volume = 1u << 0;
    constexpr std::uint32_t sound_attenuation = 1u << 1;
    /// The entity is a short and the channel a byte of its own.
    constexpr std::uint32_t sound_large_entity = 1u << 3;
    /// The sound is a short.
    constexpr std::uint32_t sound_large_sound = 1u << 4;

    // Of a baseline and of a static entity in their newer form.
    constexpr std::uint32_t state_large_model = 1u << 0;
    constexpr std::uint32_t state_large_frame = 1u << 1;
    constexpr std::uint32_t state_alpha = 1u << 2;
    constexpr std::uint32_t state_scale = 1u << 3;

    // Of what a server says of the player.
    constexpr std::uint32_t client_view_height = 1u << 0;
    constexpr std::uint32_t client_ideal_pitch = 1u << 1;
    /// The first of three, one for each angle.
    constexpr std::uint32_t client_punch = 1u << 2;
    /// The first of three, one for each axis.
    constexpr std::uint32_t client_velocity = 1u << 5;
    constexpr std::uint32_t client_on_ground = 1u << 10;
    constexpr std::uint32_t client_in_water = 1u << 11;
    constexpr std::uint32_t client_weapon_frame = 1u << 12;
    constexpr std::uint32_t client_armor = 1u << 13;
    constexpr std::uint32_t client_weapon = 1u << 14;
    /// A third byte of bits follows.
    constexpr std::uint32_t client_extend_1 = 1u << 15;
    // The high bytes of numbers that outgrew one.
    constexpr std::uint32_t client_weapon_2 = 1u << 16;
    constexpr std::uint32_t client_armor_2 = 1u << 17;
    constexpr std::uint32_t client_ammo_2 = 1u << 18;
    constexpr std::uint32_t client_shells_2 = 1u << 19;
    constexpr std::uint32_t client_nails_2 = 1u << 20;
    constexpr std::uint32_t client_rockets_2 = 1u << 21;
    constexpr std::uint32_t client_cells_2 = 1u << 22;
    /// A fourth byte of bits follows.
    constexpr std::uint32_t client_extend_2 = 1u << 23;
    constexpr std::uint32_t client_weapon_frame_2 = 1u << 24;
    constexpr std::uint32_t client_weapon_alpha = 1u << 25;

    /// What a server writes for the count of the particles of an explosion.
    constexpr std::int32_t particle_count_of_explosion = 255;

    /// A byte with a sign.
    float ReadChar(ByteReader &reader)
    {
      return static_cast<float>(static_cast<std::int8_t>(reader.ReadU8()));
    }

    constexpr std::uint32_t Bit(const DemoEntityBit bit)
    {
      return static_cast<std::uint32_t>(bit);
    }
  }

  float DemoMessageReader::ReadCoord(ByteReader &reader) const
  {
    if ((_protocol.flags & DemoProtocol::float_coord) != 0) { return reader.ReadF32(); }
    if ((_protocol.flags & DemoProtocol::int32_coord) != 0)
    {
      return static_cast<float>(reader.ReadI32()) / 16.0f;
    }
    if ((_protocol.flags & DemoProtocol::coord_24_bit) != 0)
    {
      const auto whole = static_cast<float>(reader.ReadI16());
      return whole + static_cast<float>(reader.ReadU8()) / 255.0f;
    }
    return static_cast<float>(reader.ReadI16()) / 8.0f;
  }

  float DemoMessageReader::ReadAngle(ByteReader &reader) const
  {
    if ((_protocol.flags & DemoProtocol::float_angle) != 0) { return reader.ReadF32(); }
    if ((_protocol.flags & DemoProtocol::short_angle) != 0)
    {
      return static_cast<float>(reader.ReadI16()) * (360.0f / 65536.0f);
    }
    return ReadChar(reader) * (360.0f / 256.0f);
  }

  LevelVector DemoMessageReader::ReadPlace(ByteReader &reader) const
  {
    LevelVector place;
    for (float &coordinate : place) { coordinate = ReadCoord(reader); }
    return place;
  }

  const std::string &DemoMessageReader::ReadText(ByteReader &reader)
  {
    _text.clear();
    while (true)
    {
      const std::uint8_t byte = reader.ReadU8();
      if (byte == 0 || !reader.IsGood()) { break; }
      if (_text.size() < max_text_size) { _text.push_back(static_cast<char>(byte)); }
    }
    return _text;
  }

  bool DemoMessageReader::ReadServerInfo(ByteReader &reader, DemoMessageHandler &handler, std::string &problem)
  {
    DemoServerInfo info;
    info.protocol.version = reader.ReadI32();
    if (reader.IsGood() && !DemoProtocol::IsKnown(info.protocol.version))
    {
      problem = std::format(
        "The server speaks protocol {}, and only {}, {}, and {} can be read", info.protocol.version,
        DemoProtocol::netquake, DemoProtocol::fitzquake, DemoProtocol::rmq);
      return false;
    }
    if (info.protocol.version == DemoProtocol::rmq) { info.protocol.flags = reader.ReadU32(); }

    info.max_clients = reader.ReadU8();
    if (reader.IsGood() && info.max_clients < 1)
    {
      problem = "The server says it is for no player at all";
      return false;
    }
    info.game_type = reader.ReadU8();
    info.level_name = ReadText(reader);

    // the models, then the sounds: names up to one that is empty
    for (std::vector<std::string> *names : {&info.model_names, &info.sound_names})
    {
      names->emplace_back();
      while (reader.IsGood())
      {
        const std::string &name = ReadText(reader);
        if (name.empty()) { break; }
        if (names->size() >= max_names)
        {
          problem = std::format("The server names more than {} models or sounds", max_names);
          return false;
        }
        names->push_back(name);
      }
    }
    if (!reader.IsGood()) { return true; }

    _protocol = info.protocol;
    _max_clients = info.max_clients;
    handler.HandleServerInfo(info);
    return true;
  }

  void DemoMessageReader::ReadSound(ByteReader &reader, DemoMessageHandler &handler) const
  {
    DemoSound sound;
    const std::uint32_t bits = reader.ReadU8();
    if ((bits & sound_volume) != 0) { sound.volume = static_cast<float>(reader.ReadU8()) / 255.0f; }
    if ((bits & sound_attenuation) != 0) { sound.attenuation = static_cast<float>(reader.ReadU8()) / 64.0f; }

    if ((bits & sound_large_entity) != 0)
    {
      sound.entity = reader.ReadU16();
      sound.channel = reader.ReadU8();
    }
    else
    {
      // both in one short: the channel is the lowest three bits
      const std::uint16_t packed = reader.ReadU16();
      sound.entity = packed >> 3;
      sound.channel = packed & 7;
    }
    sound.sound = (bits & sound_large_sound) != 0 ? reader.ReadU16() : reader.ReadU8();
    sound.origin = ReadPlace(reader);

    if (reader.IsGood()) { handler.HandleSound(sound); }
  }

  void DemoMessageReader::ReadClientData(ByteReader &reader, DemoMessageHandler &handler) const
  {
    DemoClientData data;
    std::uint32_t bits = reader.ReadU16();
    if ((bits & client_extend_1) != 0) { bits |= static_cast<std::uint32_t>(reader.ReadU8()) << 16; }
    if ((bits & client_extend_2) != 0) { bits |= static_cast<std::uint32_t>(reader.ReadU8()) << 24; }

    if ((bits & client_view_height) != 0) { data.view_height = ReadChar(reader); }
    if ((bits & client_ideal_pitch) != 0) { data.ideal_pitch = ReadChar(reader); }
    for (std::size_t axis = 0; axis < 3; axis++)
    {
      if ((bits & (client_punch << axis)) != 0) { data.punch_angles[axis] = ReadChar(reader); }
      if ((bits & (client_velocity << axis)) != 0) { data.velocity[axis] = ReadChar(reader) * 16.0f; }
    }

    // the items are always there, whatever their bit says
    data.items = reader.ReadU32();
    data.is_on_ground = (bits & client_on_ground) != 0;
    data.is_in_water = (bits & client_in_water) != 0;

    if ((bits & client_weapon_frame) != 0) { data.weapon_frame = reader.ReadU8(); }
    if ((bits & client_armor) != 0) { data.armor = reader.ReadU8(); }
    if ((bits & client_weapon) != 0) { data.weapon_model = reader.ReadU8(); }
    data.health = reader.ReadI16();
    data.ammo = reader.ReadU8();
    data.shells = reader.ReadU8();
    data.nails = reader.ReadU8();
    data.rockets = reader.ReadU8();
    data.cells = reader.ReadU8();
    data.active_weapon = reader.ReadU8();

    // the high bytes of what outgrew a byte
    if ((bits & client_weapon_2) != 0) { data.weapon_model |= reader.ReadU8() << 8; }
    if ((bits & client_armor_2) != 0) { data.armor |= reader.ReadU8() << 8; }
    if ((bits & client_ammo_2) != 0) { data.ammo |= reader.ReadU8() << 8; }
    if ((bits & client_shells_2) != 0) { data.shells |= reader.ReadU8() << 8; }
    if ((bits & client_nails_2) != 0) { data.nails |= reader.ReadU8() << 8; }
    if ((bits & client_rockets_2) != 0) { data.rockets |= reader.ReadU8() << 8; }
    if ((bits & client_cells_2) != 0) { data.cells |= reader.ReadU8() << 8; }
    if ((bits & client_weapon_frame_2) != 0) { data.weapon_frame |= reader.ReadU8() << 8; }
    if ((bits & client_weapon_alpha) != 0) { data.weapon_alpha = reader.ReadU8(); }

    if (reader.IsGood()) { handler.HandleClientData(data); }
  }

  bool DemoMessageReader::ReadEntityUpdate(
    ByteReader &reader, const std::uint32_t first_byte, DemoMessageHandler &handler, std::string &problem) const
  {
    DemoEntityUpdate update;
    update.bits = first_byte;
    if (update.Has(DemoEntityBit::MoreBits)) { update.bits |= static_cast<std::uint32_t>(reader.ReadU8()) << 8; }
    if (_protocol.HasExtensions())
    {
      if (update.Has(DemoEntityBit::Extend1)) { update.bits |= static_cast<std::uint32_t>(reader.ReadU8()) << 16; }
      if (update.Has(DemoEntityBit::Extend2)) { update.bits |= static_cast<std::uint32_t>(reader.ReadU8()) << 24; }
    }
    else if (update.Has(DemoEntityBit::Extend1) && reader.IsGood())
    {
      // the original protocol has no such bit: a server of Nehahra sets
      // it, and floats follow that are not read here
      problem = "The update of an entity has a bit that protocol 15 has not, as a server of Nehahra sets it";
      return false;
    }

    update.entity = update.Has(DemoEntityBit::LongEntity) ? reader.ReadU16() : reader.ReadU8();
    if (update.Has(DemoEntityBit::Model)) { update.model = reader.ReadU8(); }
    if (update.Has(DemoEntityBit::Frame)) { update.frame = reader.ReadU8(); }
    if (update.Has(DemoEntityBit::Colormap)) { update.colormap = reader.ReadU8(); }
    if (update.Has(DemoEntityBit::Skin)) { update.skin = reader.ReadU8(); }
    if (update.Has(DemoEntityBit::Effects)) { update.effects = reader.ReadU8(); }

    // a coordinate and its angle take turns
    constexpr std::uint32_t origin_bits[] = {
      Bit(DemoEntityBit::Origin1), Bit(DemoEntityBit::Origin2), Bit(DemoEntityBit::Origin3),
    };
    constexpr std::uint32_t angle_bits[] = {
      Bit(DemoEntityBit::Angle1), Bit(DemoEntityBit::Angle2), Bit(DemoEntityBit::Angle3),
    };
    for (std::size_t axis = 0; axis < 3; axis++)
    {
      if ((update.bits & origin_bits[axis]) != 0) { update.origin[axis] = ReadCoord(reader); }
      if ((update.bits & angle_bits[axis]) != 0) { update.angles[axis] = ReadAngle(reader); }
    }

    if (_protocol.HasExtensions())
    {
      if (update.Has(DemoEntityBit::Alpha)) { update.alpha = reader.ReadU8(); }
      if (update.Has(DemoEntityBit::Scale)) { update.scale = reader.ReadU8(); }
      if (update.Has(DemoEntityBit::Frame2)) { update.frame_high = reader.ReadU8(); }
      if (update.Has(DemoEntityBit::Model2)) { update.model_high = reader.ReadU8(); }
      if (update.Has(DemoEntityBit::LerpFinish))
      {
        update.lerp_finish = static_cast<float>(reader.ReadU8()) / 255.0f;
      }
    }

    if (reader.IsGood()) { handler.HandleEntityUpdate(update); }
    return true;
  }

  DemoEntityState DemoMessageReader::ReadEntityState(ByteReader &reader, const bool has_flags) const
  {
    DemoEntityState state;
    const std::uint32_t flags = has_flags ? reader.ReadU8() : 0;
    state.model = (flags & state_large_model) != 0 ? reader.ReadU16() : reader.ReadU8();
    state.frame = (flags & state_large_frame) != 0 ? reader.ReadU16() : reader.ReadU8();
    state.colormap = reader.ReadU8();
    state.skin = reader.ReadU8();
    for (std::size_t axis = 0; axis < 3; axis++)
    {
      state.origin[axis] = ReadCoord(reader);
      state.angles[axis] = ReadAngle(reader);
    }
    if ((flags & state_alpha) != 0) { state.alpha = reader.ReadU8(); }
    if ((flags & state_scale) != 0) { state.scale = reader.ReadU8(); }
    return state;
  }

  bool DemoMessageReader::ReadTempEntity(ByteReader &reader, DemoMessageHandler &handler, std::string &problem) const
  {
    const std::int32_t number = reader.ReadU8();
    const auto kind = static_cast<TempEntityKind>(number);
    switch (kind)
    {
      case TempEntityKind::Spike:
      case TempEntityKind::SuperSpike:
      case TempEntityKind::Gunshot:
      case TempEntityKind::Explosion:
      case TempEntityKind::TarExplosion:
      case TempEntityKind::WizardSpike:
      case TempEntityKind::KnightSpike:
      case TempEntityKind::LavaSplash:
      case TempEntityKind::Teleport:
      {
        const TempEntityPoint effect = {.kind = kind, .position = ReadPlace(reader)};
        if (reader.IsGood()) { handler.HandlePointEffect(effect); }
        return true;
      }
      case TempEntityKind::Explosion2:
      {
        TempEntityExplosion explosion;
        explosion.position = ReadPlace(reader);
        explosion.color_start = reader.ReadU8();
        explosion.color_count = reader.ReadU8();
        if (reader.IsGood()) { handler.HandleColoredExplosion(explosion); }
        return true;
      }
      case TempEntityKind::Lightning1:
      case TempEntityKind::Lightning2:
      case TempEntityKind::Lightning3:
      case TempEntityKind::Beam:
      {
        TempEntityBeam beam;
        beam.kind = kind;
        beam.entity = reader.ReadU16();
        beam.start = ReadPlace(reader);
        beam.end = ReadPlace(reader);
        if (reader.IsGood()) { handler.HandleBeamEffect(beam); }
        return true;
      }
    }

    // how long it is cannot be known
    if (!reader.IsGood()) { return true; }

    problem = std::format("Temp entity {} is not known", number);
    return false;
  }

  bool DemoMessageReader::ReadPlayer(ByteReader &reader, std::int32_t &player, std::string &problem) const
  {
    player = reader.ReadU8();
    if (reader.IsGood() && player >= _max_clients)
    {
      problem = std::format("The scoreboard is told of player {}, and the server is for {}", player, _max_clients);
      return false;
    }
    return true;
  }

  bool DemoMessageReader::ReadMessage(
    ByteReader &reader, const std::int32_t kind, DemoMessageHandler &handler, bool &is_over, std::string &problem)
  {
    // each reads what its kind carries and tells the handler only when all
    // of it was there: Read() sees to what was not
    switch (static_cast<DemoMessageKind>(kind))
    {
      case DemoMessageKind::Nop: return true;
      case DemoMessageKind::Disconnect:
      {
        is_over = true;
        handler.HandleDisconnect();
        return true;
      }
      case DemoMessageKind::UpdateStat:
      {
        const std::int32_t stat = reader.ReadU8();
        const std::int32_t value = reader.ReadI32();
        if (reader.IsGood()) { handler.HandleStat(stat, value); }
        return true;
      }
      case DemoMessageKind::Version:
      {
        const std::int32_t version = reader.ReadI32();
        if (!reader.IsGood()) { return true; }
        if (!DemoProtocol::IsKnown(version))
        {
          problem = std::format("The server changes to protocol {}, which cannot be read", version);
          return false;
        }
        _protocol.version = version;
        return true;
      }
      case DemoMessageKind::SetView:
      {
        const std::int32_t entity = reader.ReadU16();
        if (reader.IsGood()) { handler.HandleViewEntity(entity); }
        return true;
      }
      case DemoMessageKind::Sound:
      {
        ReadSound(reader, handler);
        return true;
      }
      case DemoMessageKind::Time:
      {
        const float seconds = reader.ReadF32();
        if (reader.IsGood()) { handler.HandleTime(seconds); }
        return true;
      }
      case DemoMessageKind::Print:
      {
        const std::string &text = ReadText(reader);
        if (reader.IsGood()) { handler.HandlePrint(text); }
        return true;
      }
      case DemoMessageKind::StuffText:
      {
        const std::string &text = ReadText(reader);
        if (reader.IsGood()) { handler.HandleStuffText(text); }
        return true;
      }
      case DemoMessageKind::SetAngle:
      {
        LevelVector angles;
        for (float &angle : angles) { angle = ReadAngle(reader); }
        if (reader.IsGood()) { handler.HandleViewAngles(angles); }
        return true;
      }
      case DemoMessageKind::ServerInfo: return ReadServerInfo(reader, handler, problem);
      case DemoMessageKind::LightStyle:
      {
        const std::int32_t style = reader.ReadU8();
        const std::string &text = ReadText(reader);
        if (reader.IsGood()) { handler.HandleLightStyle(style, text); }
        return true;
      }
      case DemoMessageKind::UpdateName:
      {
        std::int32_t player = 0;
        if (!ReadPlayer(reader, player, problem)) { return false; }
        const std::string &name = ReadText(reader);
        if (reader.IsGood()) { handler.HandlePlayerName(player, name); }
        return true;
      }
      case DemoMessageKind::UpdateFrags:
      {
        std::int32_t player = 0;
        if (!ReadPlayer(reader, player, problem)) { return false; }
        const std::int32_t frags = reader.ReadI16();
        if (reader.IsGood()) { handler.HandlePlayerFrags(player, frags); }
        return true;
      }
      case DemoMessageKind::ClientData:
      {
        ReadClientData(reader, handler);
        return true;
      }
      case DemoMessageKind::StopSound:
      {
        const std::uint16_t packed = reader.ReadU16();
        if (reader.IsGood()) { handler.HandleStopSound(packed >> 3, packed & 7); }
        return true;
      }
      case DemoMessageKind::UpdateColors:
      {
        std::int32_t player = 0;
        if (!ReadPlayer(reader, player, problem)) { return false; }
        const std::int32_t colors = reader.ReadU8();
        if (reader.IsGood()) { handler.HandlePlayerColors(player, colors); }
        return true;
      }
      case DemoMessageKind::Particle:
      {
        DemoParticles particles;
        particles.origin = ReadPlace(reader);
        for (float &part : particles.direction) { part = ReadChar(reader) / 16.0f; }
        particles.count = reader.ReadU8();
        particles.color = reader.ReadU8();
        if (particles.count == particle_count_of_explosion) { particles.count = 1024; }
        if (reader.IsGood()) { handler.HandleParticles(particles); }
        return true;
      }
      case DemoMessageKind::Damage:
      {
        DemoDamage damage;
        damage.armor = reader.ReadU8();
        damage.blood = reader.ReadU8();
        damage.from = ReadPlace(reader);
        if (reader.IsGood()) { handler.HandleDamage(damage); }
        return true;
      }
      case DemoMessageKind::SpawnStatic:
      case DemoMessageKind::SpawnStatic2:
      {
        const DemoEntityState state =
          ReadEntityState(reader, static_cast<DemoMessageKind>(kind) == DemoMessageKind::SpawnStatic2);
        if (reader.IsGood()) { handler.HandleStaticEntity(state); }
        return true;
      }
      case DemoMessageKind::SpawnBaseline:
      case DemoMessageKind::SpawnBaseline2:
      {
        const std::int32_t entity = reader.ReadU16();
        const DemoEntityState state =
          ReadEntityState(reader, static_cast<DemoMessageKind>(kind) == DemoMessageKind::SpawnBaseline2);
        if (reader.IsGood()) { handler.HandleBaseline(entity, state); }
        return true;
      }
      case DemoMessageKind::TempEntity: return ReadTempEntity(reader, handler, problem);
      case DemoMessageKind::SetPause:
      {
        const bool is_paused = reader.ReadU8() != 0;
        if (reader.IsGood()) { handler.HandlePause(is_paused); }
        return true;
      }
      case DemoMessageKind::SignonNum:
      {
        const std::int32_t stage = reader.ReadU8();
        if (reader.IsGood()) { handler.HandleSignon(stage); }
        return true;
      }
      case DemoMessageKind::CenterPrint:
      {
        const std::string &text = ReadText(reader);
        if (reader.IsGood()) { handler.HandleCenterPrint(text); }
        return true;
      }
      case DemoMessageKind::KilledMonster:
      {
        handler.HandleKilledMonster();
        return true;
      }
      case DemoMessageKind::FoundSecret:
      {
        handler.HandleFoundSecret();
        return true;
      }
      case DemoMessageKind::SpawnStaticSound:
      case DemoMessageKind::SpawnStaticSound2:
      {
        DemoStaticSound sound;
        sound.origin = ReadPlace(reader);
        sound.sound = static_cast<DemoMessageKind>(kind) == DemoMessageKind::SpawnStaticSound2
                        ? reader.ReadU16()
                        : reader.ReadU8();
        sound.volume = static_cast<float>(reader.ReadU8()) / 255.0f;
        sound.attenuation = static_cast<float>(reader.ReadU8()) / 64.0f;
        if (reader.IsGood()) { handler.HandleStaticSound(sound); }
        return true;
      }
      case DemoMessageKind::Intermission:
      {
        handler.HandleIntermission();
        return true;
      }
      case DemoMessageKind::Finale:
      {
        const std::string &text = ReadText(reader);
        if (reader.IsGood()) { handler.HandleFinale(text); }
        return true;
      }
      case DemoMessageKind::CdTrack:
      {
        const std::int32_t track = reader.ReadU8();
        const std::int32_t loop_track = reader.ReadU8();
        if (reader.IsGood()) { handler.HandleMusicTrack(track, loop_track); }
        return true;
      }
      case DemoMessageKind::SellScreen:
      {
        handler.HandleSellScreen();
        return true;
      }
      case DemoMessageKind::Cutscene:
      {
        const std::string &text = ReadText(reader);
        if (reader.IsGood()) { handler.HandleCutscene(text); }
        return true;
      }
      case DemoMessageKind::Skybox:
      {
        const std::string &name = ReadText(reader);
        if (reader.IsGood()) { handler.HandleSkybox(name); }
        return true;
      }
      case DemoMessageKind::BonusFlash:
      {
        handler.HandleBonusFlash();
        return true;
      }
      case DemoMessageKind::Fog:
      {
        DemoFog fog;
        fog.density = static_cast<float>(reader.ReadU8()) / 255.0f;
        fog.red = static_cast<float>(reader.ReadU8()) / 255.0f;
        fog.green = static_cast<float>(reader.ReadU8()) / 255.0f;
        fog.blue = static_cast<float>(reader.ReadU8()) / 255.0f;
        fog.seconds = std::max(0.0f, static_cast<float>(reader.ReadI16()) / 100.0f);
        if (reader.IsGood()) { handler.HandleFog(fog); }
        return true;
      }
    }

    problem = std::format("Message {} is not known", kind);
    return false;
  }

  bool DemoMessageReader::Read(
    const std::span<const std::uint8_t> message, DemoMessageHandler &handler, std::string &problem)
  {
    ByteReader reader(message);
    std::int32_t kind_before = 0;
    while (reader.GetPosition() < reader.GetSize())
    {
      const std::size_t start = reader.GetPosition();
      const std::uint32_t first_byte = reader.ReadU8();
      const bool is_update = (first_byte & static_cast<std::uint32_t>(DemoEntityBit::Signal)) != 0;
      const auto kind = static_cast<std::int32_t>(first_byte);

      bool is_over = false;
      std::string why;
      const bool is_read = is_update
                             ? ReadEntityUpdate(reader, first_byte, handler, why)
                             : ReadMessage(reader, kind, handler, is_over, why);
      if (!is_read)
      {
        problem = std::format(
          "{}, at byte {} of a packet of {} bytes, after message {}", why, start, message.size(), kind_before);
        return false;
      }
      if (!reader.IsGood())
      {
        problem = std::format(
          "Message {} at byte {} of a packet of {} bytes is cut short", kind, start, message.size());
        return false;
      }
      if (is_over) { return true; }

      kind_before = kind;
    }
    return true;
  }

  const DemoProtocol &DemoMessageReader::GetProtocol() const
  {
    return _protocol;
  }

  void DemoMessageReader::Reset()
  {
    _protocol = {};
    _max_clients = 0;
  }
} // quake
