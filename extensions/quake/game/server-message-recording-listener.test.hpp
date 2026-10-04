#ifndef QUAKE_SERVER_MESSAGE_RECORDING_LISTENER_TEST_HPP
#define QUAKE_SERVER_MESSAGE_RECORDING_LISTENER_TEST_HPP

#include <cstddef>
#include <cstdint>
#include <format>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "level-vector.hpp"
#include "server-message-listener.hpp"
#include "server-message-target.hpp"
#include "temp-entity-beam.hpp"
#include "temp-entity-explosion.hpp"
#include "temp-entity-point.hpp"

namespace quake
{
  /// A listener for the tests, which writes down every message it is told
  /// of: a line for each, with whom it is for and what it carried.
  class ServerMessageRecordingListener final : public ServerMessageListener
  {
    /// Whom a message is for, such as `to 1 for 3`.
    static std::string To(const ServerMessageTarget &target)
    {
      return std::format("to {} for {}", static_cast<std::int32_t>(target.destination), target.client);
    }

    static std::string Place(const LevelVector &place)
    {
      return std::format("{} {} {}", place[0], place[1], place[2]);
    }

    void Add(const std::string_view name, const ServerMessageTarget &target, const std::string_view what = {})
    {
      counts[std::string(name)]++;
      messages.push_back(
        what.empty() ? std::format("{} {}", name, To(target)) : std::format("{} {}: {}", name, To(target), what));
    }

  public:
    /// Every message in the order it came, such as `intermission to 2 for 0`.
    std::vector<std::string> messages;

    /// How often each kind of message came, by the first word of its line.
    /// A temp entity has its kind after the word, such as `point 3`.
    std::map<std::string, std::size_t> counts;

    /// The lines of the messages that were unknown or broken.
    std::vector<std::string> problems;

    void PointEffect(const ServerMessageTarget &target, const TempEntityPoint &effect) override
    {
      Add(std::format("point {}", static_cast<std::int32_t>(effect.kind)), target, Place(effect.position));
    }

    void ColoredExplosion(const ServerMessageTarget &target, const TempEntityExplosion &explosion) override
    {
      Add("explosion", target, std::format(
            "{}, colours {} and {}", Place(explosion.position), explosion.color_start, explosion.color_count));
    }

    void BeamEffect(const ServerMessageTarget &target, const TempEntityBeam &beam) override
    {
      Add(std::format("beam {}", static_cast<std::int32_t>(beam.kind)), target, std::format(
            "entity {} from {} to {}", beam.entity, Place(beam.start), Place(beam.end)));
    }

    void IntermissionStarted(const ServerMessageTarget &target) override
    {
      Add("intermission", target);
    }

    void FinaleStarted(const ServerMessageTarget &target, const std::string_view text) override
    {
      Add("finale", target, text);
    }

    void CutsceneStarted(const ServerMessageTarget &target, const std::string_view text) override
    {
      Add("cutscene", target, text);
    }

    void SellScreenShown(const ServerMessageTarget &target) override
    {
      Add("sellscreen", target);
    }

    void MusicTrackSet(
      const ServerMessageTarget &target, const std::int32_t track, const std::int32_t loop_track) override
    {
      Add("cdtrack", target, std::format("{} then {}", track, loop_track));
    }

    void MonsterKilled(const ServerMessageTarget &target) override
    {
      Add("killedmonster", target);
    }

    void SecretFound(const ServerMessageTarget &target) override
    {
      Add("foundsecret", target);
    }

    void StatSet(const ServerMessageTarget &target, const std::int32_t stat, const std::int32_t value) override
    {
      Add("updatestat", target, std::format("{} is {}", stat, value));
    }

    void ViewAnglesSet(const ServerMessageTarget &target, const LevelVector &angles) override
    {
      Add("setangle", target, Place(angles));
    }

    void ViewEntitySet(const ServerMessageTarget &target, const std::int32_t entity) override
    {
      Add("setview", target, std::format("{}", entity));
    }

    void CenterTextPrinted(const ServerMessageTarget &target, const std::string_view text) override
    {
      Add("centerprint", target, text);
    }

    void TextPrinted(const ServerMessageTarget &target, const std::string_view text) override
    {
      Add("print", target, text);
    }

    void CommandGiven(const ServerMessageTarget &target, const std::string_view text) override
    {
      Add("stufftext", target, text);
    }

    void UnknownMessage(const ServerMessageTarget &target, const std::int32_t first_byte) override
    {
      Add("unknown", target, std::format("{}", first_byte));
      problems.push_back(messages.back());
    }

    void BrokenMessage(
      const ServerMessageTarget &target, const std::int32_t first_byte, const std::string_view why) override
    {
      Add("broken", target, std::format("{}, {}", first_byte, why));
      problems.push_back(messages.back());
    }
  };
} // quake

#endif //QUAKE_SERVER_MESSAGE_RECORDING_LISTENER_TEST_HPP
