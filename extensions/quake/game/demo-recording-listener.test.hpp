#ifndef QUAKE_DEMO_RECORDING_LISTENER_TEST_HPP
#define QUAKE_DEMO_RECORDING_LISTENER_TEST_HPP

#include <cstddef>
#include <cstdint>
#include <format>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "demo-listener.hpp"

namespace quake
{
  /// A listener for the tests, which writes down everything it is told
  /// of: a line for each thing, with what it carried.
  class DemoRecordingListener final : public DemoListener
  {
    static std::string Place(const LevelVector &place)
    {
      return std::format("{} {} {}", place[0], place[1], place[2]);
    }

    void Add(const std::string_view name, const std::string_view what = {})
    {
      counts[std::string(name)]++;
      events.push_back(what.empty() ? std::string(name) : std::format("{}: {}", name, what));
    }

  public:
    /// Everything in the order it came, such as `sound: 1 2 3 1 1 at 0 0 0`.
    std::vector<std::string> events;

    /// How often each kind of thing came, by the first word of its line.
    std::map<std::string, std::size_t> counts;

    /// The levels that began.
    std::vector<DemoServerInfo> levels;

    [[nodiscard]] std::size_t CountOf(const std::string &name) const
    {
      const auto found = counts.find(name);
      return found == counts.end() ? 0 : found->second;
    }

    void LevelBegan(const DemoServerInfo &info) override
    {
      levels.push_back(info);
      Add("level", info.level_name);
    }

    void SoundStarted(const DemoSound &sound) override
    {
      Add("sound", std::format(
            "{} {} {} {} {} at {}", sound.entity, sound.channel, sound.sound, sound.volume, sound.attenuation,
            Place(sound.origin)));
    }

    void SoundStopped(const std::int32_t entity, const std::int32_t channel) override
    {
      Add("stop", std::format("{} {}", entity, channel));
    }

    void MusicTrackSet(const std::int32_t track, const std::int32_t loop_track) override
    {
      Add("music", std::format("{} {}", track, loop_track));
    }

    void PointEffect(const TempEntityPoint &effect) override
    {
      Add("point", std::format("{} at {}", static_cast<std::int32_t>(effect.kind), Place(effect.position)));
    }

    void ColoredExplosion(const TempEntityExplosion &explosion) override
    {
      Add("explosion", std::format(
            "{} {} at {}", explosion.color_start, explosion.color_count, Place(explosion.position)));
    }

    void BeamEffect(const TempEntityBeam &beam) override
    {
      Add("beam", std::format(
            "{} of {} from {} to {}", static_cast<std::int32_t>(beam.kind), beam.entity, Place(beam.start),
            Place(beam.end)));
    }

    void ParticlesBurst(const DemoParticles &particles) override
    {
      Add("particles", std::format(
            "{} of {} at {} to {}", particles.count, particles.color, Place(particles.origin),
            Place(particles.direction)));
    }

    void DamageTaken(const DemoDamage &damage) override
    {
      Add("damage", std::format("{} {} from {}", damage.armor, damage.blood, Place(damage.from)));
    }

    void BonusFlashed() override
    {
      Add("flash");
    }

    void TextPrinted(const std::string_view text) override
    {
      Add("print", text);
    }

    void CenterTextPrinted(const std::string_view text) override
    {
      Add("center", text);
    }

    void CommandGiven(const std::string_view text) override
    {
      Add("command", text);
    }

    void IntermissionStarted() override
    {
      Add("intermission");
    }

    void FinaleStarted(const std::string_view text) override
    {
      Add("finale", text);
    }

    void CutsceneStarted(const std::string_view text) override
    {
      Add("cutscene", text);
    }

    void SellScreenShown() override
    {
      Add("sell");
    }

    void FogSet(const DemoFog &fog) override
    {
      Add("fog", std::format("{} {} {} {} over {}", fog.density, fog.red, fog.green, fog.blue, fog.seconds));
    }

    void SkyboxSet(const std::string_view name) override
    {
      Add("sky", name);
    }
  };
} // quake

#endif //QUAKE_DEMO_RECORDING_LISTENER_TEST_HPP
