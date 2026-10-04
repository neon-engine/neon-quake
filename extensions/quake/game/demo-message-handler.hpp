#ifndef QUAKE_DEMO_MESSAGE_HANDLER_HPP
#define QUAKE_DEMO_MESSAGE_HANDLER_HPP

#include <cstdint>
#include <string_view>

#include "demo-client-data.hpp"
#include "demo-damage.hpp"
#include "demo-entity-state.hpp"
#include "demo-entity-update.hpp"
#include "demo-fog.hpp"
#include "demo-particles.hpp"
#include "demo-server-info.hpp"
#include "demo-sound.hpp"
#include "demo-static-sound.hpp"
#include "level-vector.hpp"
#include "temp-entity-beam.hpp"
#include "temp-entity-explosion.hpp"
#include "temp-entity-point.hpp"

namespace quake
{
  /// Who is handed the messages `DemoMessageReader` reads: a method for
  /// each kind of message, called once for each, in the order they are in
  /// the packet, with what the message carries. This is the client's side
  /// of the original: `DemoPlayer` is one, and keeps what a client keeps.
  ///
  /// Every method does nothing unless the handler says otherwise. A text
  /// holds only until the method returns.
  class DemoMessageHandler
  {
  public:
    virtual ~DemoMessageHandler() = default;

    // The level and the clock.

    /// A level begins: everything of the level before is void.
    virtual void HandleServerInfo(const DemoServerInfo &info) {}

    /// The time of the server, in seconds since the level began. What
    /// follows in the packet is of that time.
    virtual void HandleTime(float seconds) {}

    /// How far the client is with joining the level, from 1 to 3.
    virtual void HandleSignon(std::int32_t stage) {}

    /// The game stands still, or goes on.
    virtual void HandlePause(bool is_paused) {}

    /// The server is gone. Nothing of the packet is read after it.
    virtual void HandleDisconnect() {}

    // The entities.

    /// What an entity is when an update says nothing else.
    virtual void HandleBaseline(std::int32_t entity, const DemoEntityState &state) {}

    virtual void HandleEntityUpdate(const DemoEntityUpdate &update) {}

    /// An entity that never changes and has no number.
    virtual void HandleStaticEntity(const DemoEntityState &state) {}

    // The view and the player.

    /// The entity the player sees through.
    virtual void HandleViewEntity(std::int32_t entity) {}

    /// Where the player looks was set: pitch, yaw, and roll in degrees.
    virtual void HandleViewAngles(const LevelVector &angles) {}

    virtual void HandleClientData(const DemoClientData &data) {}

    /// A number of the status bar was set: which, a byte, and its value.
    virtual void HandleStat(std::int32_t stat, std::int32_t value) {}

    virtual void HandleDamage(const DemoDamage &damage) {}

    /// The screen flashes, as when an item is picked up.
    virtual void HandleBonusFlash() {}

    // What is heard.

    virtual void HandleSound(const DemoSound &sound) {}

    virtual void HandleStopSound(std::int32_t entity, std::int32_t channel) {}

    virtual void HandleStaticSound(const DemoStaticSound &sound) {}

    /// The music: the track to play, and the track to go on with.
    virtual void HandleMusicTrack(std::int32_t track, std::int32_t loop_track) {}

    // What is shown for a moment.

    virtual void HandlePointEffect(const TempEntityPoint &effect) {}

    virtual void HandleColoredExplosion(const TempEntityExplosion &explosion) {}

    virtual void HandleBeamEffect(const TempEntityBeam &beam) {}

    virtual void HandleParticles(const DemoParticles &particles) {}

    // The look of the level.

    /// How the lights of a style flicker: a letter for each tenth of a
    /// second, from `a`, dark, to `z`, twice as bright as usual.
    virtual void HandleLightStyle(std::int32_t style, std::string_view text) {}

    /// The sky is the six pictures of this name.
    virtual void HandleSkybox(std::string_view name) {}

    virtual void HandleFog(const DemoFog &fog) {}

    // Text.

    /// A line for the console.
    virtual void HandlePrint(std::string_view text) {}

    /// A text for the middle of the screen.
    virtual void HandleCenterPrint(std::string_view text) {}

    /// A command for the console of the player.
    virtual void HandleStuffText(std::string_view text) {}

    // The scoreboard and the counts.

    virtual void HandlePlayerName(std::int32_t player, std::string_view name) {}

    virtual void HandlePlayerFrags(std::int32_t player, std::int32_t frags) {}

    virtual void HandlePlayerColors(std::int32_t player, std::int32_t colors) {}

    virtual void HandleKilledMonster() {}

    virtual void HandleFoundSecret() {}

    // The end of a level and of an episode.

    virtual void HandleIntermission() {}

    virtual void HandleFinale(std::string_view text) {}

    virtual void HandleCutscene(std::string_view text) {}

    virtual void HandleSellScreen() {}
  };
} // quake

#endif //QUAKE_DEMO_MESSAGE_HANDLER_HPP
