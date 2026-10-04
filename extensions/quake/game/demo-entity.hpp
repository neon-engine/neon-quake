#ifndef QUAKE_DEMO_ENTITY_HPP
#define QUAKE_DEMO_ENTITY_HPP

#include <cstdint>

#include "demo-entity-state.hpp"

namespace quake
{
  /// An entity a `DemoPlayer` shows now: what the server said of it last,
  /// with its place and angles worked out for the clock of the player.
  struct DemoEntity
  {
    /// Which entity of the server. It stays the same for as long as the
    /// entity lives, so a host knows it again in the next frame.
    std::int32_t number = 0;

    /// The model, frame, skin, colormap, effects, alpha, and scale as the
    /// last update has them, and the origin and angles between the last
    /// two updates.
    DemoEntityState state{};

    /// Whether the entity moves in steps, as monsters do. Its place is
    /// then moved from step to step over a tenth of a second.
    bool moves_in_steps = false;

    /// Whether the entity was not shown just before, or jumped too far to
    /// have moved there: a trail behind it starts anew.
    bool is_placed_anew = false;

    /// When, on the clock of the player, the next frame of the model is
    /// due, so that a host blends frames over the right time. Zero when
    /// the server did not say: a tenth of a second after the frame came.
    float frame_finish_time = 0.0f;
  };
} // quake

#endif //QUAKE_DEMO_ENTITY_HPP
