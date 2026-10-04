#ifndef QUAKE_GAME_CODE_HPP
#define QUAKE_GAME_CODE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <neon/extension/neon-extension.hpp>

#include "formats/bsp-file.hpp"
#include "formats/qc-machine.hpp"
#include "game-data.hpp"
#include "game/level-collision.hpp"
#include "game/level-physics.hpp"
#include "game/level-running.hpp"
#include "game/level-stepping.hpp"
#include "game/level-touching.hpp"
#include "game/qc-core-builtins.hpp"
#include "game/qc-fields.hpp"
#include "game/qc-globals.hpp"
#include "game/qc-host.hpp"
#include "game/qc-world-builtins.hpp"
#include "level-view.hpp"
#include "model-view.hpp"
#include "sound-view.hpp"

namespace quake
{
  /// The game code of the game, `progs.dat`, run for a level that is shown:
  /// what the engine of the original was to it, on Neon Engine.
  ///
  /// The game code owns the game. It says what every entity of a level is,
  /// where it is, and which model it shows, in the fields of its entities.
  /// This reads those fields after every step and makes the world of the
  /// engine agree: a door stands where its `origin` says, a monster shows
  /// the frame its `frame` says. The other way around it tells the game
  /// code where the player of the scene is, and what that player touches,
  /// which is what opens a door and picks an item up.
  ///
  /// What the game code asks that needs the walls of a level, a line traced
  /// through it, a monster that walks, an item that falls, is answered by
  /// the collision of the game itself, `LevelCollision` and what is built
  /// on it, with the hulls of the level as the original has them. The one
  /// thing that is not moved so is the player, whose body is the engine's.
  class GameCode final : public QcHost
  {
    using Vector = std::array<float, 3>;

    /// What there is of the game code while a level runs, in the order it
    /// is made.
    struct Level
    {
      BspFile file;
      QcMachine machine;
      QcGlobals globals;
      QcFields fields;
      QcCoreBuiltins builtins;
      LevelRunning running;

      // what the entities collide with, and what moves them through it
      LevelCollision collision;
      LevelTouching touching;
      LevelStepping stepping;
      LevelPhysics physics;
      QcWorldBuiltins world_builtins;

      Level(Progs progs, QcHost &host);
    };

    /// What the engine shows for an entity of the game code.
    struct Shown
    {
      neon::extension::Entity entity = 0;

      /// The model it shows, as the game code names it. Empty for nothing.
      std::string model;

      /// Whether the entity of the engine is a model of the level, which the
      /// level view made, and not one made here.
      bool is_part = false;

      /// Whether it is a model of the kind ModelView shows.
      bool is_alias = false;

      Vector origin{};
      Vector angles{};
      bool is_placed = false;
    };

    std::unique_ptr<Level> _level;
    std::vector<Shown> _shown;

    // what the game is shown with, which outlive a level
    const neon::extension::World *_world = nullptr;
    const GameData *_data = nullptr;
    LevelView *_view = nullptr;
    ModelView *_models = nullptr;
    SoundView *_sounds = nullptr;

    NeonField _position_field{};
    NeonField _rotation_field{};

    // The player of the scene, once it is there, and where the game code
    // was last told it is.
    neon::extension::Entity _player = 0;
    bool _player_placed = false;
    Vector _player_origin{};

    // what the game code printed of a line that has not ended yet
    std::string _line;

    // The camera of the player, and the entity under it that shows the
    // weapon in the player's hands.
    neon::extension::Entity _camera = 0;
    neon::extension::Entity _weapon = 0;

    // What the player asked for since the last step: the number of a
    // weapon, which the game code is handed once.
    float _impulse = 0.0f;

    // how many failures of the game code were said already
    std::size_t _failures_said = 0;

    /// The entity of the game code the one player is, the first after the
    /// world, as in the original.
    static constexpr std::int32_t player_entity = 1;

    /// How far the middle of the body of the player is above where the game
    /// has the player, see LevelView.
    static constexpr float middle_height = 4.0f;

    /// How far from a wall that is touched the player may stand, in units
    /// of the game: the body of the engine keeps a little off what it walks
    /// into.
    static constexpr float touch_reach = 2.0f;

    /// What the game code is handed to go to the next weapon the player
    /// has, and to the one before.
    static constexpr float next_weapon_impulse = 10.0f;
    static constexpr float previous_weapon_impulse = 12.0f;

    /// Registers the builtins that need the level or the engine.
    void RegisterBuiltins();

    /// What `setmodel` does: the entity names a model and takes the size of
    /// one of the level.
    void SetModel(std::int32_t entity, std::int32_t name_offset, std::string_view name);

    /// What `makestatic` does: what the entity shows stays for the rest of
    /// the level, and the entity itself goes.
    void MakeStatic(std::int32_t entity);

    /// Takes away what the engine shows for an entity.
    void Hide(Shown &shown);

    /// Makes what the engine shows for an entity agree with its fields.
    void Show(std::int32_t entity);

    /// Tells the game code where the player of the scene is.
    void ReadPlayer(float dt);

    /// Puts the player of the scene where the game code moved it to, as a
    /// teleporter does, and at the start of a level.
    void PlacePlayer();

    /// Shows the weapon the game code has in the player's hands, with the
    /// frame it is at, in front of the camera.
    void ShowWeapon();

    /// Calls `touch` of everything the player is in or at.
    void TouchAsPlayer();

    /// Says the failures of the game code that were not said yet.
    void SayFailures();

  public:
    // QcHost
    void PrintToAll(std::string_view text) override;

    void PrintToClient(std::int32_t client, std::string_view text) override;

    void PrintToCenter(std::int32_t client, std::string_view text) override;

    void Error(std::int32_t self, std::string_view text) override;

    void ObjectError(std::int32_t entity, std::string_view text) override;

    void EntityRemoved(std::int32_t entity) override;

    void ChangeLevel(std::string_view level) override;

    /// Starts the game code for a level the level view shows already: reads
    /// `progs.dat`, hands it the entities of the level, and lets the one
    /// player in. Returns false, and says why in `error`, when the data has
    /// no game code or it cannot be read; the level is then shown without.
    bool Start(
      const neon::extension::World &world,
      const GameData &data,
      LevelView &view,
      ModelView &models,
      SoundView &sounds,
      const std::string &map,
      std::string &error);

    /// Takes what the player pressed in this frame that the game code is
    /// told of once, in its next step: the number of a weapon, or the wish
    /// for the next one or the one before.
    void ReadInput(const neon::extension::World &world);

    /// Whether a level runs.
    [[nodiscard]] bool IsRunning() const;

    /// Lets a step of time pass for the game: the player is read, the game
    /// code runs, and the world of the engine is made to agree.
    void Advance(const neon::extension::World &world, float dt);
  };
} // quake

#endif //QUAKE_GAME_CODE_HPP
