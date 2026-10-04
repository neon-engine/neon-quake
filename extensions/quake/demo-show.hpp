#ifndef QUAKE_DEMO_SHOW_HPP
#define QUAKE_DEMO_SHOW_HPP

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <neon/extension/neon-extension.hpp>

#include "game-data.hpp"
#include "game/demo-listener.hpp"
#include "game/demo-player.hpp"
#include "level-view.hpp"
#include "model-view.hpp"
#include "sound-view.hpp"
#include "sprite-view.hpp"

namespace quake
{
  /// Shows a recorded game: what the original plays behind its menu until a
  /// player starts a game of their own.
  ///
  /// A recording is what a server told a client. `DemoPlayer` reads it and
  /// keeps what a client knows: the level, the entities and where they are
  /// now, where the view is. This shows that with what shows a game that is
  /// played: the level, the models, the sprites, the sounds. No game code
  /// runs, and nothing collides.
  ///
  /// Left out for now is what a recording shows for a moment: particles,
  /// the bolts of lightning, the light of an explosion.
  class DemoShow final : DemoListener
  {
    /// What an entity of the recording is shown by.
    struct Shown
    {
      neon::extension::Entity entity = 0;
      std::string model;
      bool is_part = false;
      bool is_alias = false;
      bool is_sprite = false;

      /// Whether the recording named it in the frame that is shown.
      bool is_there = false;
    };

    DemoPlayer _player{*this};

    const neon::extension::World *_world = nullptr;
    const GameData *_data = nullptr;
    LevelView *_view = nullptr;
    ModelView *_models = nullptr;
    SoundView *_sounds = nullptr;
    SpriteView *_sprites = nullptr;

    // the entity everything of the recording stands under, and the entities
    // of the scene the view is: who turns around, and who looks up and down
    neon::extension::Entity _root = 0;
    neon::extension::Entity _eyes = 0;
    neon::extension::Entity _camera = 0;

    // what shows each entity, by its number
    std::map<std::int32_t, Shown> _shown;

    // how many of what stands still and of what sounds without end were
    // shown already: the recording names them while the level begins
    std::size_t _static_entities = 0;
    std::size_t _static_sounds = 0;

    // the styles of the lights as the level view was told them
    std::array<std::string, 64> _styles;

    // an entity of the engine is known by its name, so each gets a number
    std::int64_t _made = 0;

    bool _has_level = false;

    /// The bit of the effects of a model that has it turn around by itself.
    static constexpr std::uint32_t turns_flag = 8;

    /// How far out of sight a model of the level is put that is not there.
    static constexpr float out_of_sight = -10000.0f;

    void LevelBegan(const DemoServerInfo &info) override;

    void SoundStarted(const DemoSound &sound) override;

    void MusicTrackSet(std::int32_t track, std::int32_t loop_track) override;

    /// Shows a model at a place, with an entity of its own when `shown` has
    /// none yet. Returns false for a model that cannot be shown.
    bool Show(Shown &shown, const DemoEntityState &state);

    void Hide(Shown &shown);

    void ShowView();

  public:
    /// Starts the recording of a name, such as `demo1.dem`. Returns false
    /// and says why when the data has none or it cannot be read.
    bool Start(
      const neon::extension::World &world,
      const GameData &data,
      LevelView &view,
      ModelView &models,
      SoundView &sounds,
      SpriteView &sprites,
      const std::string &name,
      std::string &problem);

    /// Plays on by the seconds a frame took, and shows what there is now.
    void Update(const neon::extension::World &world, float seconds);

    /// Whether the recording has ended, or broke.
    [[nodiscard]] bool IsOver() const;

    /// Takes everything the recording showed out of the world. The level
    /// stays until another is shown.
    void Stop(const neon::extension::World &world);
  };
} // quake

#endif //QUAKE_DEMO_SHOW_HPP
