#ifndef QUAKE_SERVER_MESSAGE_KIND_HPP
#define QUAKE_SERVER_MESSAGE_KIND_HPP

#include <cstdint>

namespace quake
{
  /// The messages of the network the game code writes itself, by the number
  /// of their first byte in version 15 of the protocol. The engine of the
  /// original writes many more, which the game code has no part in, and
  /// which are not here.
  enum class ServerMessageKind : std::int32_t
  {
    /// A number of the status bar: a byte for which, and a long.
    UpdateStat = 3,
    /// The entity the player sees through.
    SetView = 5,
    /// A line of text for the console: a string.
    Print = 8,
    /// A command for the console of the player: a string.
    StuffText = 9,
    /// Where the player looks: three angles.
    SetAngle = 10,
    /// Something shown for a moment: a byte for its kind, see
    /// `TempEntityKind`, and what that kind needs.
    TempEntity = 23,
    /// A text in the middle of the screen: a string.
    CenterPrint = 26,
    /// A monster was killed.
    KilledMonster = 27,
    /// A secret was found.
    FoundSecret = 28,
    /// The level is over, and its tally is shown.
    Intermission = 30,
    /// An episode is over: the text that is shown, a string.
    Finale = 31,
    /// The music: a byte for the track, and one for the track to go on with.
    CdTrack = 32,
    /// The screen that asks to buy the whole game.
    SellScreen = 33,
    /// A text over the view of a camera: a string.
    Cutscene = 34,
  };
} // quake

#endif //QUAKE_SERVER_MESSAGE_KIND_HPP
