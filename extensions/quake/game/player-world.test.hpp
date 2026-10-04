#ifndef QUAKE_PLAYER_WORLD_TEST_HPP
#define QUAKE_PLAYER_WORLD_TEST_HPP

#include <cstddef>
#include <cstdint>

#include "formats/bsp-file.hpp"
#include "formats/bsp-hull-builder.test.hpp"
#include "level-world.test.hpp"

namespace quake
{
  /// A small level for the tests of how a player moves, with what
  /// `LevelWorld` lacks for them: water, and a step that is too high by a
  /// little.
  ///
  /// The level is the same along Y. Along X it is, from west to east:
  ///
  /// - a pool, west of `pool_edge`, whose floor is at `pool_floor` and
  ///   whose water stands up to `water_surface`, a little under the floor
  ///   next to it,
  /// - a floor at height 0, up to `step_edge`,
  /// - a step, `step_height` high, which a player walks up, to
  ///   `high_step_edge`,
  /// - a second step on top of it, `high_step_height` higher, which a
  ///   player does not walk up,
  /// - and east of `wall_edge` a wall, solid all the way up.
  ///
  /// Model 1 is the slab of `LevelWorld`, for a lift.
  class PlayerWorld
  {
    /// The profile of the world as a tree, as in `LevelWorld`. `pool` is
    /// what is above the floor of the pool: the water and the air over it
    /// for a point, and what is empty for a box, which no water stops.
    template <typename Fork>
    static std::int16_t Profile(
      const Fork &fork, const std::int16_t solid, const std::int16_t empty, const std::int16_t pool,
      const float grow_x, const float grow_z)
    {
      const BspVector up{0.0f, 0.0f, 1.0f};
      const BspVector east{1.0f, 0.0f, 0.0f};
      const auto floor = [&](const float height) { return fork(up, height + grow_z, empty, solid); };

      std::int16_t tree = fork(east, wall_edge - grow_x, solid, floor(step_height + high_step_height));
      tree = fork(east, high_step_edge - grow_x, tree, floor(step_height));
      tree = fork(east, step_edge - grow_x, tree, floor(0.0f));
      return fork(east, pool_edge - grow_x, tree, fork(up, pool_floor + grow_z, pool, solid));
    }

    /// A box as a tree: solid inside, empty around it.
    template <typename Fork>
    static std::int16_t Box(
      const Fork &fork, const std::int16_t solid, const std::int16_t empty, const BspVector &mins,
      const BspVector &maxs)
    {
      std::int16_t next = solid;
      next = fork(BspVector{0.0f, 0.0f, 1.0f}, maxs.z, empty, next);
      next = fork(BspVector{0.0f, 0.0f, 1.0f}, mins.z, next, empty);
      next = fork(BspVector{0.0f, 1.0f, 0.0f}, maxs.y, empty, next);
      next = fork(BspVector{0.0f, 1.0f, 0.0f}, mins.y, next, empty);
      next = fork(BspVector{1.0f, 0.0f, 0.0f}, maxs.x, empty, next);
      return fork(BspVector{1.0f, 0.0f, 0.0f}, mins.x, next, empty);
    }

  public:
    static constexpr float pool_edge = -100.0f;
    static constexpr float pool_floor = -300.0f;
    static constexpr float water_surface = -10.0f;
    static constexpr float step_edge = 100.0f;
    static constexpr float step_height = 16.0f;
    static constexpr float high_step_edge = 200.0f;
    static constexpr float high_step_height = 20.0f;
    static constexpr float wall_edge = 300.0f;

    /// The level, with the world and the slab.
    [[nodiscard]] static BspFile MakeLevel()
    {
      BspHullBuilder builder;
      const auto node = [&builder](const BspVector &normal, const float distance, const std::int16_t front,
                                   const std::int16_t back)
      {
        return builder.Node(normal, distance, front, back);
      };
      const auto clip_node = [&builder](const BspVector &normal, const float distance, const std::int16_t front,
                                        const std::int16_t back)
      {
        return builder.ClipNode(normal, distance, front, back);
      };
      constexpr std::int16_t solid_leaf = BspHullBuilder::solid_leaf;
      constexpr std::int16_t empty_leaf = BspHullBuilder::empty_leaf;
      constexpr std::int16_t solid = BspHullBuilder::solid;
      constexpr std::int16_t empty = BspHullBuilder::empty;

      // only a point knows the water
      const std::int16_t water = node(
        BspVector{0.0f, 0.0f, 1.0f}, water_surface, empty_leaf, builder.Leaf(BspHullBuilder::water));
      const std::int16_t world_point = Profile(node, solid_leaf, empty_leaf, water, 0.0f, 0.0f);
      const std::int16_t world_player = Profile(clip_node, solid, empty, empty, 16.0f, 24.0f);
      const std::int16_t world_large = Profile(clip_node, solid, empty, empty, 32.0f, 24.0f);
      builder.Model(world_point, world_player, world_large);

      // the slab, as in `LevelWorld`
      constexpr float half = LevelWorld::slab_half_width;
      const BspVector slab_mins{-half, -half, -LevelWorld::slab_thickness};
      const BspVector slab_maxs{half, half, 0.0f};
      const std::int16_t slab_point = Box(node, solid_leaf, empty_leaf, slab_mins, slab_maxs);
      const std::int16_t slab_player = Box(
        clip_node, solid, empty,
        {slab_mins.x - 16.0f, slab_mins.y - 16.0f, slab_mins.z - 32.0f},
        {slab_maxs.x + 16.0f, slab_maxs.y + 16.0f, slab_maxs.z + 24.0f});
      const std::int16_t slab_large = Box(
        clip_node, solid, empty,
        {slab_mins.x - 32.0f, slab_mins.y - 32.0f, slab_mins.z - 64.0f},
        {slab_maxs.x + 32.0f, slab_maxs.y + 32.0f, slab_maxs.z + 24.0f});
      const std::size_t slab = builder.Model(slab_point, slab_player, slab_large);
      builder.file.models[slab].mins = slab_mins;
      builder.file.models[slab].maxs = slab_maxs;
      return builder.file;
    }
  };
} // quake

#endif //QUAKE_PLAYER_WORLD_TEST_HPP
