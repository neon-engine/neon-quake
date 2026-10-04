#ifndef QUAKE_SOUND_VIEW_HPP
#define QUAKE_SOUND_VIEW_HPP

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <neon/extension/neon-extension.hpp>

#include "game-data.hpp"

namespace quake
{
  /// Plays the sounds of the game, `sound/*.wav`, in the world of the
  /// engine.
  ///
  /// A sound is a file of the archives. It is handed to the audio of the
  /// engine the first time it is asked for, and played by an entity with a
  /// `SoundSource` where the game code says it sounds. Such an entity goes
  /// when its sound has ended.
  ///
  /// The game code plays a sound on a channel of an entity of its own. A
  /// sound on a channel that still plays one takes its place, which is how
  /// the game cuts a weapon's sound short with the next; channel 0 never
  /// takes a place.
  ///
  /// How far a sound carries is the game's: it gets quieter in a straight
  /// line, down to silence a thousand units away, sooner the more it is
  /// said to wear off, and a sound that does not wear off is heard the same
  /// everywhere.
  class SoundView
  {
    /// A sound that plays.
    struct Playing
    {
      neon::extension::Entity entity = 0;

      /// The entity of the game code it sounds from, and its channel there.
      std::int32_t owner = 0;
      std::int32_t channel = 0;

      /// Whether it starts over for as long as the level is there.
      bool is_ambient = false;
    };

    // The path a sound source plays each sound by, by the name the game
    // code has for it. Empty for one the data does not hold or the audio
    // did not take, so that it is looked for, and said, once.
    std::map<std::string, std::string> _paths;

    std::vector<Playing> _playing;

    // how many entities were made to play a sound, which numbers them
    std::uint64_t _made = 0;

    /// How far a sound that wears off by 1 is heard, in units of the game.
    static constexpr float reach = 1000.0f;

    /// By how much less a sound of the surroundings wears off than one that
    /// is played, for the same number.
    static constexpr float ambient_reach = 64.0f;

    /// The path of a sound by its name, such as `doors/drclos4.wav`,
    /// handing it to the audio when it was not yet.
    const std::string &Find(const neon::extension::World &world, const GameData &data, const std::string &name);

    /// Makes the entity that plays a sound at a place of the engine. `far`
    /// is how far it is heard, in metres, and 0 for the same everywhere.
    neon::extension::Entity Make(
      const neon::extension::World &world,
      const std::string &name,
      const std::string &path,
      const neon::extension::Vector3 &place,
      float volume,
      float far,
      bool loops);

  public:
    /// Plays a sound once, where an entity of the game code is, on one of
    /// its channels. `place` is in the space of the engine. `volume` is
    /// from 0 to 1, and `wears_off` the number the game code gives: 0 for
    /// heard everywhere, 1 for an ordinary sound.
    void Play(
      const neon::extension::World &world,
      const GameData &data,
      std::int32_t owner,
      std::int32_t channel,
      const std::string &name,
      const neon::extension::Vector3 &place,
      float volume,
      float wears_off);

    /// Plays a sound of the surroundings, over and over, at a place of the
    /// engine, until the level ends.
    void PlayAmbient(
      const neon::extension::World &world,
      const GameData &data,
      const std::string &name,
      const neon::extension::Vector3 &place,
      float volume,
      float wears_off);

    /// Takes away the entities whose sound has ended.
    void Update(const neon::extension::World &world);

    /// Stops everything, when a level ends.
    void Clear(const neon::extension::World &world);
  };
} // quake

#endif //QUAKE_SOUND_VIEW_HPP
