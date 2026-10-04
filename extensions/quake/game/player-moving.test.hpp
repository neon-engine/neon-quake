#ifndef QUAKE_PLAYER_MOVING_TEST_HPP
#define QUAKE_PLAYER_MOVING_TEST_HPP

#include <cstdint>
#include <memory>
#include <string_view>

#include "formats/bsp-file.hpp"
#include "level-moving.test.hpp"
#include "player-command.hpp"
#include "player-movement.hpp"
#include "player-world.test.hpp"

namespace quake
{
  /// The level of `PlayerWorld` set up as a host sets a level up that has
  /// the player moved as the original moves one: the mover walks the
  /// players, and a `PlayerMovement` steers them.
  class PlayerMovingTest : public LevelMovingTest
  {
  protected:
    /// The length of a frame of the original at its usual rate.
    static constexpr float frame_time = 1.0f / 72.0f;

    /// Where a player who stands on a floor at height 0 is.
    static constexpr float standing = 24.0f + epsilon;

    std::unique_ptr<PlayerMovement> _movement;

    void SetUp() override
    {
      // what a player has and `LevelWorld` does not add
      ProgsBuilder &builder = _program.builder;
      for (const std::string_view name : {"v_angle", "punchangle", "oldorigin", "movedir"})
      {
        builder.Field(name, ProgsType::Vector);
      }
      builder.Field("fixangle", ProgsType::Float);
      builder.Field("teleport_time", ProgsType::Float);

      LevelMovingTest::SetUp();
      _movement = std::make_unique<PlayerMovement>(*_collision);
      _physics->SetWalksClients(true);
    }

    [[nodiscard]] BspFile MakeLevel() const override
    {
      return PlayerWorld::MakeLevel();
    }

    /// Makes a player who is alive at a place, as the game code makes one.
    std::int32_t MakePlayer(const LevelVector &origin)
    {
      const std::int32_t player = Make(origin, LevelWorld::player_mins, LevelWorld::player_maxs, QcSolid::SlideBox);
      _fields->movetype.Set(*_machine, player, static_cast<float>(QcMoveType::Walk));
      _fields->health.Set(*_machine, player, 100.0f);
      _fields->view_ofs.Set(*_machine, player, {0.0f, 0.0f, 22.0f});
      Set(player, QcFlag::Client);
      return player;
    }

    [[nodiscard]] LevelVector VelocityOf(const std::int32_t entity) const
    {
      return _fields->velocity.Get(*_machine, entity);
    }

    /// Steers a player once, at the time of the level.
    void Steer(const std::int32_t player, const PlayerCommand &command, const float dt = frame_time) const
    {
      _movement->Steer(player, command, static_cast<float>(_running->GetTime()), dt);
    }

    /// Lets a number of frames pass as a host does: the player is
    /// steered, then everything moves.
    void Play(
      const std::int32_t player, const PlayerCommand &command, const int frames, const float dt = frame_time) const
    {
      for (int frame = 0; frame < frames; frame++)
      {
        Steer(player, command, dt);
        _running->Advance(dt);
      }
    }
  };
} // quake

#endif //QUAKE_PLAYER_MOVING_TEST_HPP
