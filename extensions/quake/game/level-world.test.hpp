#ifndef QUAKE_LEVEL_WORLD_TEST_HPP
#define QUAKE_LEVEL_WORLD_TEST_HPP

#include <cstdint>
#include <string_view>

#include "formats/bsp-file.hpp"
#include "formats/bsp-hull-builder.test.hpp"
#include "formats/qc-machine.hpp"
#include "level-program.test.hpp"
#include "level-vector.hpp"
#include "qc-builtin-number.hpp"
#include "qc-fields.hpp"
#include "qc-solid.hpp"

namespace quake
{
  /// A small level and a small program for the tests of what collides and
  /// moves in a level.
  ///
  /// The level is the same along Y. Along X it is, from west to east:
  ///
  /// - a pit, west of `pit_edge`, whose floor is at `pit_floor`,
  /// - a floor at height 0, up to `step_edge`,
  /// - a step, `step_height` high, which a monster walks up, to `ledge_edge`,
  /// - a ledge at `ledge_height`, too high to walk up, to `wall_edge`,
  /// - and east of that a wall, solid all the way up.
  ///
  /// Model 1 is a slab for a lift, `slab_half_width` to each side and
  /// `slab_thickness` thick, whose top is where its entity stands.
  class LevelWorld
  {
    /// The profile of the world as a tree. `fork` adds a fork of a plane
    /// and names it; the sides and the floors are moved by what a hull is
    /// grown by.
    template <typename Fork>
    static std::int16_t Profile(
      const Fork &fork, const std::int16_t solid, const std::int16_t empty, const float grow_x, const float grow_z)
    {
      const auto floor = [&](const float height)
      {
        return fork(BspVector{0.0f, 0.0f, 1.0f}, height + grow_z, empty, solid);
      };
      const BspVector east{1.0f, 0.0f, 0.0f};
      std::int16_t tree = fork(east, wall_edge - grow_x, solid, floor(ledge_height));
      tree = fork(east, ledge_edge - grow_x, tree, floor(step_height));
      tree = fork(east, step_edge - grow_x, tree, floor(0.0f));
      return fork(east, pit_edge - grow_x, tree, floor(pit_floor));
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
    static constexpr float pit_edge = -100.0f;
    static constexpr float pit_floor = -100.0f;
    static constexpr float step_edge = 100.0f;
    static constexpr float step_height = 16.0f;
    static constexpr float ledge_edge = 200.0f;
    static constexpr float ledge_height = 64.0f;
    static constexpr float wall_edge = 300.0f;

    static constexpr float slab_half_width = 32.0f;
    static constexpr float slab_thickness = 8.0f;

    /// The box of a player, which is the one hull 1 of a level is for.
    static constexpr LevelVector player_mins = {-16.0f, -16.0f, -24.0f};
    static constexpr LevelVector player_maxs = {16.0f, 16.0f, 32.0f};

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

      // the world: a point, the box of a player, and the box of a large
      // monster, each counted from the lowest corner of its box
      const std::int16_t world_point = Profile(node, solid_leaf, empty_leaf, 0.0f, 0.0f);
      const std::int16_t world_player = Profile(clip_node, solid, empty, 16.0f, 24.0f);
      const std::int16_t world_large = Profile(clip_node, solid, empty, 32.0f, 24.0f);
      builder.Model(world_point, world_player, world_large);

      const BspVector slab_mins{-slab_half_width, -slab_half_width, -slab_thickness};
      const BspVector slab_maxs{slab_half_width, slab_half_width, 0.0f};
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

    /// Adds to a program the globals and fields that colliding and moving
    /// work with, and the builtins of the world as functions of their
    /// names, so that a test calls `traceline` as game code does.
    static void AddTo(LevelProgram &program)
    {
      ProgsBuilder &builder = program.builder;
      for (const std::string_view name : {
             "trace_allsolid", "trace_startsolid", "trace_fraction", "trace_plane_dist", "trace_inopen",
             "trace_inwater",
           })
      {
        builder.Name(builder.Float(), name, ProgsType::Float);
      }
      builder.Name(builder.Vector(), "trace_endpos", ProgsType::Vector);
      builder.Name(builder.Vector(), "trace_plane_normal", ProgsType::Vector);
      builder.Name(builder.Integer(), "trace_ent", ProgsType::Entity);

      for (const std::string_view name : {"mins", "maxs", "size", "absmin", "absmax", "view_ofs"})
      {
        builder.Field(name, ProgsType::Vector);
      }
      for (const std::string_view name : {
             "flags", "ideal_yaw", "yaw_speed", "watertype", "waterlevel", "gravity",
           })
      {
        builder.Field(name, ProgsType::Float);
      }
      for (const std::string_view name : {"groundentity", "chain", "enemy", "goalentity"})
      {
        builder.Field(name, ProgsType::Entity);
      }
      builder.Field("touch", ProgsType::Function);
      builder.Field("blocked", ProgsType::Function);

      for (const QcBuiltinNumber number : {
             QcBuiltinNumber::TraceLine, QcBuiltinNumber::CheckClient, QcBuiltinNumber::FindRadius,
             QcBuiltinNumber::WalkMove, QcBuiltinNumber::DropToFloor, QcBuiltinNumber::CheckBottom,
             QcBuiltinNumber::PointContents, QcBuiltinNumber::Aim, QcBuiltinNumber::ChangeYaw,
             QcBuiltinNumber::MoveToGoal,
           })
      {
        builder.Builtin(QcBuiltinName(number), static_cast<std::int32_t>(number));
      }
    }

    /// Makes an entity with a box around a place that stops things in a
    /// way.
    static std::int32_t MakeEntity(
      QcMachine &machine, const LevelVector &origin, const LevelVector &mins, const LevelVector &maxs,
      const QcSolid solid = QcSolid::BoundingBox)
    {
      const QcFields fields(machine.GetProgs());
      const std::int32_t entity = machine.CreateEntity().value_or(0);
      fields.origin.Set(machine, entity, origin);
      fields.mins.Set(machine, entity, mins);
      fields.maxs.Set(machine, entity, maxs);
      fields.size.Set(machine, entity, Difference(maxs, mins));
      fields.solid.Set(machine, entity, static_cast<float>(solid));
      return entity;
    }
  };
} // quake

#endif //QUAKE_LEVEL_WORLD_TEST_HPP
