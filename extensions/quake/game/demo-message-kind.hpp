#ifndef QUAKE_DEMO_MESSAGE_KIND_HPP
#define QUAKE_DEMO_MESSAGE_KIND_HPP

#include <cstdint>

namespace quake
{
  /// Every message a server sends its client, by the number of its first
  /// byte, in the protocols `DemoProtocol` names. A first byte with its
  /// highest bit set is none of these: it starts the update of an entity,
  /// see `DemoEntityUpdate`.
  ///
  /// `ServerMessageKind` has the few of these the game code writes itself.
  enum class DemoMessageKind : std::int32_t
  {
    /// Nothing, to keep a connection alive.
    Nop = 1,
    /// The server is gone: the demo is over.
    Disconnect = 2,
    /// A number of the status bar: a byte for which, and a long.
    UpdateStat = 3,
    /// The protocol, a long.
    Version = 4,
    /// The entity the player sees through, a short.
    SetView = 5,
    /// A sound starts, see `DemoSound`.
    Sound = 6,
    /// The time of the server, a float. Every packet of a running level
    /// starts with it.
    Time = 7,
    /// A line of text for the console: a string.
    Print = 8,
    /// A command for the console of the player: a string.
    StuffText = 9,
    /// Where the player looks: three angles.
    SetAngle = 10,
    /// A level begins, see `DemoServerInfo`.
    ServerInfo = 11,
    /// How a light flickers: a byte for which style, and a string.
    LightStyle = 12,
    /// The name of a player: a byte for which, and a string.
    UpdateName = 13,
    /// The frags of a player: a byte for which, and a short.
    UpdateFrags = 14,
    /// What the player has and is, see `DemoClientData`.
    ClientData = 15,
    /// A sound stops: a short with the entity and the channel.
    StopSound = 16,
    /// The colours of a player: a byte for which, and a byte.
    UpdateColors = 17,
    /// A burst of particles, see `DemoParticles`.
    Particle = 18,
    /// The player was hurt, see `DemoDamage`.
    Damage = 19,
    /// An entity that never changes, see `DemoEntityState`.
    SpawnStatic = 20,
    /// What an entity is when an update says nothing else: a short for
    /// which, and a `DemoEntityState`.
    SpawnBaseline = 22,
    /// Something shown for a moment, see `TempEntityKind`.
    TempEntity = 23,
    /// The game stands still or goes on: a byte.
    SetPause = 24,
    /// How far the client is with joining the level: a byte.
    SignonNum = 25,
    /// A text in the middle of the screen: a string.
    CenterPrint = 26,
    /// A monster was killed.
    KilledMonster = 27,
    /// A secret was found.
    FoundSecret = 28,
    /// A sound that goes on for ever at a place, see `DemoStaticSound`.
    SpawnStaticSound = 29,
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

    // What FitzQuake added.

    /// The sky is six pictures: their name, a string.
    Skybox = 37,
    /// The screen flashes, as when an item is picked up.
    BonusFlash = 40,
    /// The fog, see `DemoFog`.
    Fog = 41,
    /// As SpawnBaseline, with a byte of flags for what is written larger.
    SpawnBaseline2 = 42,
    /// As SpawnStatic, with a byte of flags for what is written larger.
    SpawnStatic2 = 43,
    /// As SpawnStaticSound, with a short for the sound.
    SpawnStaticSound2 = 44,
  };
} // quake

#endif //QUAKE_DEMO_MESSAGE_KIND_HPP
