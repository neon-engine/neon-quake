#ifndef QUAKE_DEMO_LISTENER_HPP
#define QUAKE_DEMO_LISTENER_HPP

#include <cstdint>
#include <string_view>

#include "demo-damage.hpp"
#include "demo-fog.hpp"
#include "demo-particles.hpp"
#include "demo-server-info.hpp"
#include "demo-sound.hpp"
#include "temp-entity-beam.hpp"
#include "temp-entity-explosion.hpp"
#include "temp-entity-point.hpp"

namespace quake
{
  /// Who is told of what happens at a moment while a `DemoPlayer` plays:
  /// what a host has to start, show once, or play, and cannot read from
  /// the player afterwards. What lasts, such as the entities and the
  /// numbers of the status bar, is asked of the player instead.
  ///
  /// Every method does nothing unless the listener says otherwise. A
  /// method is called from within DemoPlayer::Advance(), in the order the
  /// recording has things. A text holds only until the method returns.
  class DemoListener
  {
  public:
    virtual ~DemoListener() = default;

    /// A level begins: the host loads the map, which is the model of
    /// index 1, and forgets everything of the level before. The player
    /// has nothing but this of the level yet.
    virtual void LevelBegan(const DemoServerInfo &info) {}

    // What is heard.

    virtual void SoundStarted(const DemoSound &sound) {}

    /// The sound of a channel of an entity stops.
    virtual void SoundStopped(std::int32_t entity, std::int32_t channel) {}

    /// The music: the track to play, and the track to go on with when
    /// that one is over. When the recording forces a track, it is the
    /// track here, as in the original.
    virtual void MusicTrackSet(std::int32_t track, std::int32_t loop_track) {}

    // What is shown for a moment.

    /// A temp entity at one place: every kind but the beams and
    /// `Explosion2`.
    virtual void PointEffect(const TempEntityPoint &effect) {}

    /// The temp entity `Explosion2`, with its colours.
    virtual void ColoredExplosion(const TempEntityExplosion &explosion) {}

    /// A bolt of lightning or a beam, from an entity to a place.
    virtual void BeamEffect(const TempEntityBeam &beam) {}

    virtual void ParticlesBurst(const DemoParticles &particles) {}

    /// The player was hurt: the screen flashes, and the view is kicked.
    virtual void DamageTaken(const DemoDamage &damage) {}

    /// The screen flashes, as when an item is picked up.
    virtual void BonusFlashed() {}

    // Text.

    /// A line for the console.
    virtual void TextPrinted(std::string_view text) {}

    /// A text for the middle of the screen.
    virtual void CenterTextPrinted(std::string_view text) {}

    /// A command for the console of the player, such as `bf`, the flash
    /// of an item picked up.
    virtual void CommandGiven(std::string_view text) {}

    // The end of a level and of an episode.

    /// The level is over: its tally is shown.
    virtual void IntermissionStarted() {}

    /// An episode is over, and its text is to be shown, letter by letter.
    virtual void FinaleStarted(std::string_view text) {}

    /// As a finale, but the view stays as it is.
    virtual void CutsceneStarted(std::string_view text) {}

    /// The screen that asks to buy the whole game is to be shown.
    virtual void SellScreenShown() {}

    // The look of the level.

    virtual void FogSet(const DemoFog &fog) {}

    /// The sky is the six pictures of this name.
    virtual void SkyboxSet(std::string_view name) {}
  };
} // quake

#endif //QUAKE_DEMO_LISTENER_HPP
