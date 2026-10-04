#ifndef QUAKE_DEMO_ENTITY_STATE_HPP
#define QUAKE_DEMO_ENTITY_STATE_HPP

#include <cstdint>

#include "level-vector.hpp"

namespace quake
{
  /// What a server says of an entity as a whole: the baseline of an entity
  /// that moves, which is what it is whenever an update says nothing else,
  /// and all there is of a static entity, such as a torch on a wall.
  struct DemoEntityState
  {
    /// What `alpha` is when the server said none: the entity is as its
    /// model has it, which for water is how the player set water to look.
    static constexpr std::int32_t default_alpha = 0;

    /// What `scale` is for an entity of the size of its model.
    static constexpr std::int32_t default_scale = 16;

    /// Which model, as an index into the names of DemoServerInfo. Zero is
    /// no model: there is nothing to show.
    std::int32_t model = 0;

    std::int32_t frame = 0;
    std::int32_t colormap = 0;
    std::int32_t skin = 0;

    /// The bits of the field `effects` of the game code. A baseline has
    /// none.
    std::int32_t effects = 0;

    /// In the units and axes of the game, and in degrees.
    LevelVector origin{};
    LevelVector angles{};

    /// How much the entity is seen through, as the byte FitzQuake sends:
    /// 0 for not said, else from 1, which is not seen at all, to 255,
    /// which is solid. See Opacity().
    std::int32_t alpha = default_alpha;

    /// The size, in 16ths of that of the model, a byte of RMQ.
    std::int32_t scale = default_scale;

    /// A byte of alpha as a share from 0, not seen, to 1, solid. The byte
    /// for not said is 1.
    [[nodiscard]] static constexpr float Opacity(const std::int32_t alpha)
    {
      return alpha == default_alpha ? 1.0f : static_cast<float>(alpha - 1) / 254.0f;
    }
  };
} // quake

#endif //QUAKE_DEMO_ENTITY_STATE_HPP
