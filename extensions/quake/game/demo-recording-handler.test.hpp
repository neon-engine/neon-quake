#ifndef QUAKE_DEMO_RECORDING_HANDLER_TEST_HPP
#define QUAKE_DEMO_RECORDING_HANDLER_TEST_HPP

#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <vector>

#include "demo-message-handler.hpp"

namespace quake
{
  /// A handler for the tests, which writes down every message it is
  /// handed: a line for each, with what it carried. What has too much
  /// for a line is kept as it came too.
  class DemoRecordingHandler final : public DemoMessageHandler
  {
    static std::string Place(const LevelVector &place)
    {
      return std::format("{} {} {}", place[0], place[1], place[2]);
    }

    static std::string State(const DemoEntityState &state)
    {
      return std::format(
        "model {} frame {} colormap {} skin {} at {} turned {} alpha {} scale {}", state.model, state.frame,
        state.colormap, state.skin, Place(state.origin), Place(state.angles), state.alpha, state.scale);
    }

  public:
    /// Every message in the order it came, such as `time 1.5`.
    std::vector<std::string> messages;

    // The last of each of those that carry much.
    std::vector<DemoServerInfo> infos;
    std::vector<DemoEntityUpdate> updates;
    std::vector<DemoClientData> client_data;

    void HandleServerInfo(const DemoServerInfo &info) override
    {
      infos.push_back(info);
      messages.push_back(std::format("serverinfo {}", info.level_name));
    }

    void HandleTime(const float seconds) override
    {
      messages.push_back(std::format("time {}", seconds));
    }

    void HandleSignon(const std::int32_t stage) override
    {
      messages.push_back(std::format("signon {}", stage));
    }

    void HandlePause(const bool is_paused) override
    {
      messages.push_back(std::format("pause {}", is_paused));
    }

    void HandleDisconnect() override
    {
      messages.emplace_back("disconnect");
    }

    void HandleBaseline(const std::int32_t entity, const DemoEntityState &state) override
    {
      messages.push_back(std::format("baseline {}: {}", entity, State(state)));
    }

    void HandleEntityUpdate(const DemoEntityUpdate &update) override
    {
      updates.push_back(update);
      messages.push_back(std::format("update {} bits {:#x}", update.entity, update.bits));
    }

    void HandleStaticEntity(const DemoEntityState &state) override
    {
      messages.push_back(std::format("static: {}", State(state)));
    }

    void HandleViewEntity(const std::int32_t entity) override
    {
      messages.push_back(std::format("view {}", entity));
    }

    void HandleViewAngles(const LevelVector &angles) override
    {
      messages.push_back(std::format("angles {}", Place(angles)));
    }

    void HandleClientData(const DemoClientData &data) override
    {
      client_data.push_back(data);
      messages.emplace_back("clientdata");
    }

    void HandleStat(const std::int32_t stat, const std::int32_t value) override
    {
      messages.push_back(std::format("stat {} {}", stat, value));
    }

    void HandleDamage(const DemoDamage &damage) override
    {
      messages.push_back(std::format("damage {} {} from {}", damage.armor, damage.blood, Place(damage.from)));
    }

    void HandleBonusFlash() override
    {
      messages.emplace_back("flash");
    }

    void HandleSound(const DemoSound &sound) override
    {
      messages.push_back(std::format(
        "sound {} of {} on {} volume {} attenuation {} at {}", sound.sound, sound.entity, sound.channel,
        sound.volume, sound.attenuation, Place(sound.origin)));
    }

    void HandleStopSound(const std::int32_t entity, const std::int32_t channel) override
    {
      messages.push_back(std::format("stop {} on {}", entity, channel));
    }

    void HandleStaticSound(const DemoStaticSound &sound) override
    {
      messages.push_back(std::format(
        "static sound {} volume {} attenuation {} at {}", sound.sound, sound.volume, sound.attenuation,
        Place(sound.origin)));
    }

    void HandleMusicTrack(const std::int32_t track, const std::int32_t loop_track) override
    {
      messages.push_back(std::format("music {} {}", track, loop_track));
    }

    void HandlePointEffect(const TempEntityPoint &effect) override
    {
      messages.push_back(std::format("point {} at {}", static_cast<std::int32_t>(effect.kind), Place(effect.position)));
    }

    void HandleColoredExplosion(const TempEntityExplosion &explosion) override
    {
      messages.push_back(std::format(
        "explosion {} {} at {}", explosion.color_start, explosion.color_count, Place(explosion.position)));
    }

    void HandleBeamEffect(const TempEntityBeam &beam) override
    {
      messages.push_back(std::format(
        "beam {} of {} from {} to {}", static_cast<std::int32_t>(beam.kind), beam.entity, Place(beam.start),
        Place(beam.end)));
    }

    void HandleParticles(const DemoParticles &particles) override
    {
      messages.push_back(std::format(
        "particles {} of {} at {} to {}", particles.count, particles.color, Place(particles.origin),
        Place(particles.direction)));
    }

    void HandleLightStyle(const std::int32_t style, const std::string_view text) override
    {
      messages.push_back(std::format("style {} {}", style, text));
    }

    void HandleSkybox(const std::string_view name) override
    {
      messages.push_back(std::format("sky {}", name));
    }

    void HandleFog(const DemoFog &fog) override
    {
      messages.push_back(
        std::format("fog {} {} {} {} over {}", fog.density, fog.red, fog.green, fog.blue, fog.seconds));
    }

    void HandlePrint(const std::string_view text) override
    {
      messages.push_back(std::format("print {}", text));
    }

    void HandleCenterPrint(const std::string_view text) override
    {
      messages.push_back(std::format("center {}", text));
    }

    void HandleStuffText(const std::string_view text) override
    {
      messages.push_back(std::format("command {}", text));
    }

    void HandlePlayerName(const std::int32_t player, const std::string_view name) override
    {
      messages.push_back(std::format("name {} {}", player, name));
    }

    void HandlePlayerFrags(const std::int32_t player, const std::int32_t frags) override
    {
      messages.push_back(std::format("frags {} {}", player, frags));
    }

    void HandlePlayerColors(const std::int32_t player, const std::int32_t colors) override
    {
      messages.push_back(std::format("colors {} {}", player, colors));
    }

    void HandleKilledMonster() override
    {
      messages.emplace_back("killed");
    }

    void HandleFoundSecret() override
    {
      messages.emplace_back("secret");
    }

    void HandleIntermission() override
    {
      messages.emplace_back("intermission");
    }

    void HandleFinale(const std::string_view text) override
    {
      messages.push_back(std::format("finale {}", text));
    }

    void HandleCutscene(const std::string_view text) override
    {
      messages.push_back(std::format("cutscene {}", text));
    }

    void HandleSellScreen() override
    {
      messages.emplace_back("sell");
    }
  };
} // quake

#endif //QUAKE_DEMO_RECORDING_HANDLER_TEST_HPP
