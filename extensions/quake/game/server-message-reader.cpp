#include "server-message-reader.hpp"

#include <format>

#include "level-vector.hpp"
#include "qc-message-kind.hpp"
#include "server-message-kind.hpp"
#include "temp-entity-beam.hpp"
#include "temp-entity-explosion.hpp"
#include "temp-entity-kind.hpp"
#include "temp-entity-point.hpp"

namespace quake
{
  // What each message is made of after its first byte, which needs nothing
  // of the object.
  namespace
  {
    /// What a part of a message is to the one who reads it. A byte and a
    /// char are one to the client, and so are a short and an entity.
    enum class Part
    {
      Byte,
      Long,
      Coord,
      Angle,
      String,
      Entity,
      /// There is no such part: the message is whole before it.
      None,
    };

    constexpr std::string_view written_names[] = {
      "byte", "char", "short", "long", "coord", "angle", "string", "entity",
    };

    constexpr std::string_view part_names[] = {"byte", "long", "coord", "angle", "string", "entity", "nothing"};

    /// The name of what was written, for a log.
    std::string_view NameOf(const QcMessageKind kind)
    {
      const auto index = static_cast<std::size_t>(kind);
      return index < std::size(written_names) ? written_names[index] : "value of no kind";
    }

    /// A number of the game code as the whole number the network carries.
    /// What is no number, or too large for one, is 0.
    std::int32_t WholeNumber(const float number)
    {
      if (!(number >= -2147483648.0f && number < 2147483648.0f)) { return 0; }

      return static_cast<std::int32_t>(number);
    }

    /// A number as the byte the network carries, from 0 to 255.
    std::int32_t ByteOf(const float number)
    {
      return WholeNumber(number) & 255;
    }

    bool IsKnown(const std::int32_t first_byte)
    {
      switch (static_cast<ServerMessageKind>(first_byte))
      {
        case ServerMessageKind::UpdateStat:
        case ServerMessageKind::SetView:
        case ServerMessageKind::Print:
        case ServerMessageKind::StuffText:
        case ServerMessageKind::SetAngle:
        case ServerMessageKind::TempEntity:
        case ServerMessageKind::CenterPrint:
        case ServerMessageKind::KilledMonster:
        case ServerMessageKind::FoundSecret:
        case ServerMessageKind::Intermission:
        case ServerMessageKind::Finale:
        case ServerMessageKind::CdTrack:
        case ServerMessageKind::SellScreen:
        case ServerMessageKind::Cutscene:
        {
          return true;
        }
      }
      return false;
    }

    bool IsKnownTempEntity(const std::int32_t kind)
    {
      return kind >= static_cast<std::int32_t>(TempEntityKind::Spike) &&
        kind <= static_cast<std::int32_t>(TempEntityKind::Beam);
    }

    bool IsBeam(const TempEntityKind kind)
    {
      return kind == TempEntityKind::Lightning1 || kind == TempEntityKind::Lightning2 ||
        kind == TempEntityKind::Lightning3 || kind == TempEntityKind::Beam;
    }

    /// The part of a temp entity at a place after its first byte, counted
    /// from 0: the byte of its kind, and then what the kind needs.
    Part TempEntityPartAt(const TempEntityKind kind, const std::size_t index)
    {
      if (index == 0) { return Part::Byte; }

      // the entity it comes from, where it starts, where it ends
      if (IsBeam(kind))
      {
        if (index == 1) { return Part::Entity; }
        return index <= 7 ? Part::Coord : Part::None;
      }

      // where it is, and for the one kind its two colours
      if (index <= 3) { return Part::Coord; }
      if (kind == TempEntityKind::Explosion2 && index <= 5) { return Part::Byte; }
      return Part::None;
    }

    /// The part of a message at a place after its first byte, counted from
    /// 0. `temp_entity` is the kind of a temp entity, which is known from
    /// place 1 on.
    Part PartAt(const ServerMessageKind kind, const TempEntityKind temp_entity, const std::size_t index)
    {
      switch (kind)
      {
        case ServerMessageKind::TempEntity:
        {
          return TempEntityPartAt(temp_entity, index);
        }
        case ServerMessageKind::UpdateStat:
        {
          return index == 0 ? Part::Byte : index == 1 ? Part::Long : Part::None;
        }
        case ServerMessageKind::SetView:
        {
          return index == 0 ? Part::Entity : Part::None;
        }
        case ServerMessageKind::SetAngle:
        {
          return index <= 2 ? Part::Angle : Part::None;
        }
        case ServerMessageKind::CdTrack:
        {
          return index <= 1 ? Part::Byte : Part::None;
        }
        case ServerMessageKind::Print:
        case ServerMessageKind::StuffText:
        case ServerMessageKind::CenterPrint:
        case ServerMessageKind::Finale:
        case ServerMessageKind::Cutscene:
        {
          return index == 0 ? Part::String : Part::None;
        }
        case ServerMessageKind::KilledMonster:
        case ServerMessageKind::FoundSecret:
        case ServerMessageKind::Intermission:
        case ServerMessageKind::SellScreen:
        {
          return Part::None;
        }
      }
      return Part::None;
    }

    /// Whether what was written can be the part that is to come. The
    /// network carries a char as it does a byte, and an entity as a short
    /// with its number, so each is taken for the other.
    bool Accepts(const Part part, const QcMessageKind written)
    {
      switch (part)
      {
        case Part::Byte: return written == QcMessageKind::Byte || written == QcMessageKind::Char;
        case Part::Long: return written == QcMessageKind::Long;
        case Part::Coord: return written == QcMessageKind::Coord;
        case Part::Angle: return written == QcMessageKind::Angle;
        case Part::String: return written == QcMessageKind::String;
        case Part::Entity: return written == QcMessageKind::Entity || written == QcMessageKind::Short;
        case Part::None: return false;
      }
      return false;
    }
  }

  ServerMessageReader::ServerMessageReader(ServerMessageListener &listener)
    : _listener(listener)
  {
  }

  void ServerMessageReader::Start(
    Pending &pending, const ServerMessageTarget &target, const QcMessageValue &value)
  {
    // every message starts with a byte
    if (value.kind != QcMessageKind::Byte)
    {
      if (!pending.is_skipping)
      {
        _listener.BrokenMessage(
          target, ServerMessageListener::no_message,
          std::format("a {} came where a message is to start", NameOf(value.kind)));
      }
      pending.is_skipping = true;
      return;
    }

    const std::int32_t first_byte = ByteOf(value.number);
    if (!IsKnown(first_byte))
    {
      // while skipping, this is most likely a part of what is skipped
      if (!pending.is_skipping) { _listener.UnknownMessage(target, first_byte); }
      pending.is_skipping = true;
      return;
    }

    pending = {.is_in_message = true, .first_byte = first_byte, .client = target.client};
    if (PartAt(static_cast<ServerMessageKind>(first_byte), TempEntityKind::Spike, 0) == Part::None)
    {
      Deliver(pending, target, {});
    }
  }

  void ServerMessageReader::Break(Pending &pending, const ServerMessageTarget &target, const std::string_view why)
  {
    const std::int32_t first_byte = pending.first_byte;
    pending = {};
    _listener.BrokenMessage(target, first_byte, why);
  }

  void ServerMessageReader::Deliver(Pending &pending, const ServerMessageTarget &target, const std::string_view text)
  {
    // forgotten first: the listener may have the game code write more
    const Pending message = pending;
    pending = {};

    const std::array<float, max_numbers> &numbers = message.numbers;
    switch (static_cast<ServerMessageKind>(message.first_byte))
    {
      case ServerMessageKind::TempEntity:
      {
        const auto kind = static_cast<TempEntityKind>(ByteOf(numbers[0]));
        const LevelVector first = {numbers[1], numbers[2], numbers[3]};
        if (IsBeam(kind))
        {
          return _listener.BeamEffect(target, {
                                        .kind = kind,
                                        .entity = message.entity,
                                        .start = first,
                                        .end = {numbers[4], numbers[5], numbers[6]},
                                      });
        }
        if (kind == TempEntityKind::Explosion2)
        {
          return _listener.ColoredExplosion(target, {
                                              .position = first,
                                              .color_start = ByteOf(numbers[4]),
                                              .color_count = ByteOf(numbers[5]),
                                            });
        }
        return _listener.PointEffect(target, {.kind = kind, .position = first});
      }
      case ServerMessageKind::UpdateStat:
      {
        return _listener.StatSet(target, ByteOf(numbers[0]), WholeNumber(numbers[1]));
      }
      case ServerMessageKind::SetView: return _listener.ViewEntitySet(target, message.entity);
      case ServerMessageKind::SetAngle: return _listener.ViewAnglesSet(target, {numbers[0], numbers[1], numbers[2]});
      case ServerMessageKind::CdTrack: return _listener.MusicTrackSet(target, ByteOf(numbers[0]), ByteOf(numbers[1]));
      case ServerMessageKind::Print: return _listener.TextPrinted(target, text);
      case ServerMessageKind::StuffText: return _listener.CommandGiven(target, text);
      case ServerMessageKind::CenterPrint: return _listener.CenterTextPrinted(target, text);
      case ServerMessageKind::Finale: return _listener.FinaleStarted(target, text);
      case ServerMessageKind::Cutscene: return _listener.CutsceneStarted(target, text);
      case ServerMessageKind::KilledMonster: return _listener.MonsterKilled(target);
      case ServerMessageKind::FoundSecret: return _listener.SecretFound(target);
      case ServerMessageKind::Intermission: return _listener.IntermissionStarted(target);
      case ServerMessageKind::SellScreen: return _listener.SellScreenShown(target);
    }
  }

  void ServerMessageReader::Read(
    const QcMessageDestination destination, const std::int32_t client, const QcMessageValue &value)
  {
    const auto index = static_cast<std::size_t>(destination);
    if (index >= destination_count) { return; }

    Pending &pending = _pending[index];
    const ServerMessageTarget target = {.destination = destination, .client = client};

    // the game code turned to another player in the middle of a message
    if (pending.is_in_message && pending.client != client)
    {
      Break(pending, {.destination = destination, .client = pending.client},
            "the game code went on to another player before the message was whole");
    }
    if (!pending.is_in_message) { return Start(pending, target, value); }

    const auto kind = static_cast<ServerMessageKind>(pending.first_byte);
    const auto temp_entity = static_cast<TempEntityKind>(ByteOf(pending.numbers[0]));
    const Part part = PartAt(kind, temp_entity, pending.part_count);
    if (!Accepts(part, value.kind))
    {
      Break(pending, target, std::format(
              "a {} came where a {} is to come, as part {} after the first byte", NameOf(value.kind),
              part_names[static_cast<std::size_t>(part)], pending.part_count + 1));
      // The message may have ended early, and this be the start of the
      // next. When it is not, it is skipped, and the listener was told.
      pending.is_skipping = true;
      return Start(pending, target, value);
    }

    if (part == Part::Entity)
    {
      pending.entity = value.kind == QcMessageKind::Entity ? value.entity : WholeNumber(value.number);
    }
    else if (part != Part::String && pending.number_count < max_numbers)
    {
      pending.numbers[pending.number_count++] = value.number;
    }
    pending.part_count++;

    // the kind of a temp entity says what follows, so one that is not known
    // cannot be read on
    if (kind == ServerMessageKind::TempEntity && pending.part_count == 1 && !IsKnownTempEntity(ByteOf(value.number)))
    {
      Break(pending, target, std::format("a temp entity of kind {} is not known", ByteOf(value.number)));
      pending.is_skipping = true;
      return;
    }

    const auto now = static_cast<TempEntityKind>(ByteOf(pending.numbers[0]));
    if (PartAt(kind, now, pending.part_count) == Part::None) { Deliver(pending, target, value.text); }
  }

  bool ServerMessageReader::IsInMessage(const QcMessageDestination destination) const
  {
    const auto index = static_cast<std::size_t>(destination);
    return index < destination_count && _pending[index].is_in_message;
  }

  void ServerMessageReader::Flush()
  {
    for (std::size_t index = 0; index < destination_count; index++)
    {
      Pending &pending = _pending[index];
      if (pending.is_in_message)
      {
        const ServerMessageTarget target = {
          .destination = static_cast<QcMessageDestination>(index), .client = pending.client,
        };
        Break(pending, target, std::format(
                "the message ended after {} of its parts after the first byte", pending.part_count));
      }
      pending = {};
    }
  }
} // quake
