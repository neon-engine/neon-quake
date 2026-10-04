#ifndef QUAKE_SERVER_MESSAGE_LISTENER_HPP
#define QUAKE_SERVER_MESSAGE_LISTENER_HPP

#include <cstdint>
#include <string_view>

#include "level-vector.hpp"
#include "server-message-target.hpp"
#include "temp-entity-beam.hpp"
#include "temp-entity-explosion.hpp"
#include "temp-entity-point.hpp"

namespace quake
{
  /// Who is told of the messages `ServerMessageReader` put together: a
  /// method for each kind of message, called once for each whole message,
  /// with what the message carries. In the original these are what the
  /// client's side of the engine acts on.
  ///
  /// Every method does nothing unless the listener says otherwise. A method
  /// is called while the game code runs, from within the builtin that wrote
  /// the last part of the message. A text holds only until it returns.
  class ServerMessageListener
  {
  public:
    /// What BrokenMessage() gets for a first byte when a value came that
    /// belongs to no message.
    static constexpr std::int32_t no_message = -1;

    virtual ~ServerMessageListener() = default;

    // What is shown for a moment.

    /// A temp entity at one place: every kind but the beams and
    /// `Explosion2`.
    virtual void PointEffect(const ServerMessageTarget &target, const TempEntityPoint &effect) {}

    /// The temp entity `Explosion2`, with its colours.
    virtual void ColoredExplosion(const ServerMessageTarget &target, const TempEntityExplosion &explosion) {}

    /// A bolt of lightning or a beam, from an entity to a place.
    virtual void BeamEffect(const ServerMessageTarget &target, const TempEntityBeam &beam) {}

    // The end of a level and of an episode.

    /// The level is over: the player sees it from the camera the game code
    /// put it at, with the tally of the level.
    virtual void IntermissionStarted(const ServerMessageTarget &target) {}

    /// An episode is over, and its text is to be shown, letter by letter.
    virtual void FinaleStarted(const ServerMessageTarget &target, std::string_view text) {}

    /// As a finale, but the view stays as it is.
    virtual void CutsceneStarted(const ServerMessageTarget &target, std::string_view text) {}

    /// The screen that asks to buy the whole game is to be shown.
    virtual void SellScreenShown(const ServerMessageTarget &target) {}

    // What a player hears and counts.

    /// The music: the track to play, and the track to go on with when that
    /// one is over. Each is a byte.
    virtual void MusicTrackSet(const ServerMessageTarget &target, std::int32_t track, std::int32_t loop_track) {}

    /// A monster was killed: the count of the status bar goes up by one.
    virtual void MonsterKilled(const ServerMessageTarget &target) {}

    /// A secret was found: the count of the status bar goes up by one.
    virtual void SecretFound(const ServerMessageTarget &target) {}

    /// A number of the status bar was set: which, a byte, and its value.
    virtual void StatSet(const ServerMessageTarget &target, std::int32_t stat, std::int32_t value) {}

    // The view.

    /// Where the player looks was set: pitch, yaw, and roll in degrees.
    virtual void ViewAnglesSet(const ServerMessageTarget &target, const LevelVector &angles) {}

    /// The entity the player sees through was set.
    virtual void ViewEntitySet(const ServerMessageTarget &target, std::int32_t entity) {}

    // Text.

    /// A text for the middle of the screen.
    virtual void CenterTextPrinted(const ServerMessageTarget &target, std::string_view text) {}

    /// A line for the console.
    virtual void TextPrinted(const ServerMessageTarget &target, std::string_view text) {}

    /// A command for the console of the player.
    virtual void CommandGiven(const ServerMessageTarget &target, std::string_view text) {}

    // What could not be made sense of.

    /// A message started with a byte the reader knows no message for. What
    /// follows is skipped until a byte comes that starts a message it knows.
    virtual void UnknownMessage(const ServerMessageTarget &target, std::int32_t first_byte) {}

    /// A message did not come as its kind has it, and is dropped: a part
    /// was of the wrong kind, it ended early, or a value came that starts
    /// no message, with `no_message` for the first byte then. `why` says
    /// which, for a log.
    virtual void BrokenMessage(const ServerMessageTarget &target, std::int32_t first_byte, std::string_view why) {}
  };
} // quake

#endif //QUAKE_SERVER_MESSAGE_LISTENER_HPP
