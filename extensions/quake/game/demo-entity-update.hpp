#ifndef QUAKE_DEMO_ENTITY_UPDATE_HPP
#define QUAKE_DEMO_ENTITY_UPDATE_HPP

#include <cstdint>

#include "demo-entity-bit.hpp"
#include "level-vector.hpp"

namespace quake
{
  /// The update of one entity, as a server sends it in every packet for
  /// every entity the player may see. It carries only what differs from
  /// the baseline of the entity: a value whose bit is not set is that of
  /// the baseline, not that of the update before. An entity a packet has
  /// no update for is not shown any more.
  struct DemoEntityUpdate
  {
    /// The bits of `DemoEntityBit`: which of the values below were sent.
    std::uint32_t bits = 0;

    /// Which entity.
    std::int32_t entity = 0;

    /// The low byte of the model, with the bit Model.
    std::int32_t model = 0;

    /// The low byte of the frame, with the bit Frame.
    std::int32_t frame = 0;

    std::int32_t colormap = 0;
    std::int32_t skin = 0;
    std::int32_t effects = 0;

    /// Each of the three has a bit of its own.
    LevelVector origin{};
    LevelVector angles{};

    /// As DemoEntityState::alpha and DemoEntityState::scale.
    std::int32_t alpha = 0;
    std::int32_t scale = 0;

    /// The high bytes, with the bits Frame2 and Model2. Without the bit
    /// for the low byte, the low byte is that of the baseline.
    std::int32_t frame_high = 0;
    std::int32_t model_high = 0;

    /// In how many seconds the next frame of the model is due, from 0 to
    /// 1, with the bit LerpFinish. Not sent when it is a tenth.
    float lerp_finish = 0.0f;

    [[nodiscard]] constexpr bool Has(const DemoEntityBit bit) const
    {
      return (bits & static_cast<std::uint32_t>(bit)) != 0;
    }
  };
} // quake

#endif //QUAKE_DEMO_ENTITY_UPDATE_HPP
