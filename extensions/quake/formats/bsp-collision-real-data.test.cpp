#include "bsp-collision.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "bsp-file.hpp"
#include "entity-text.hpp"
#include "real-data.test.hpp"

namespace
{
  using quake::BspCollision;
  using quake::BspContents;
  using quake::BspEntity;
  using quake::BspFile;
  using quake::BspHull;
  using quake::BspModel;
  using quake::BspTraceResult;
  using quake::BspVector;
  using quake::EntityText;
  using quake::RealData;

  constexpr BspVector nowhere{0.0f, 0.0f, 0.0f};
  constexpr BspVector player_mins{-16.0f, -16.0f, -24.0f};
  constexpr BspVector player_maxs{16.0f, 16.0f, 32.0f};

  /// The levels of the real game that are read, each with its name. A
  /// level of another version than the original's is refused by the reader
  /// and left out.
  class BspCollisionRealDataTest : public ::testing::Test
  {
  protected:
    std::vector<std::pair<std::string, BspFile>> _levels;

    void SetUp() override
    {
      const RealData &data = RealData::Get();
      if (!data.IsThere()) { GTEST_SKIP() << data.GetProblem(); }

      for (const std::string &name : data.ListNames("maps"))
      {
        if (!name.ends_with(".bsp")) { continue; }
        BspFile file;
        std::string ignored;
        if (file.Read(data.GetBytes(name), ignored)) { _levels.emplace_back(name, std::move(file)); }
      }
      if (_levels.empty()) { GTEST_SKIP() << "No level of version 29 in the paks"; }
    }
  };

  /// The three numbers of a value such as `"0 64 128"`.
  BspVector ReadVector(const std::string &value)
  {
    BspVector vector;
    std::istringstream numbers(value);
    numbers >> vector.x >> vector.y >> vector.z;
    EXPECT_FALSE(numbers.fail()) << value;
    return vector;
  }

  /// The places a player starts a level at: the origins of its entities
  /// `info_player_start`.
  std::vector<BspVector> FindStarts(const EntityText &text)
  {
    std::vector<BspVector> starts;
    for (const BspEntity &entity : text.entities)
    {
      const std::string *class_name = entity.Find("classname");
      const std::string *origin = entity.Find("origin");
      if (class_name == nullptr || *class_name != "info_player_start" || origin == nullptr) { continue; }
      starts.push_back(ReadVector(*origin));
    }
    return starts;
  }

  /// What stands in a level and is not the world: the models of its doors,
  /// lifts, and walls that are entities, each with where its entity puts
  /// it. What is only there to be touched, a trigger, is left out, as the
  /// game leaves it out of a move.
  std::vector<std::pair<BspCollision, BspVector>> MakeSolidEntities(
    const BspFile &file, const EntityText &text, const std::string &name)
  {
    std::vector<std::pair<BspCollision, BspVector>> solids;
    for (const BspEntity &entity : text.entities)
    {
      const std::string *class_name = entity.Find("classname");
      const std::string *model = entity.Find("model");
      if (class_name == nullptr || model == nullptr || !model->starts_with("*")) { continue; }
      if (class_name->starts_with("trigger_")) { continue; }

      BspCollision collision;
      std::string error;
      const auto number = static_cast<std::size_t>(std::stoul(model->substr(1)));
      EXPECT_TRUE(collision.Build(file, number, error)) << name << ", model " << number << ": " << error;

      const std::string *origin = entity.Find("origin");
      solids.emplace_back(std::move(collision), origin == nullptr ? nowhere : ReadVector(*origin));
    }
    return solids;
  }

  TEST_F(BspCollisionRealDataTest, MakesTheHullsOfEveryModelOfEveryLevel)
  {
    for (const auto &[name, file] : _levels)
    {
      for (std::size_t model = 0; model < file.models.size(); model++)
      {
        BspCollision collision;
        std::string error;
        EXPECT_TRUE(collision.Build(file, model, error)) << name << ", model " << model << ": " << error;
      }
    }
  }

  TEST_F(BspCollisionRealDataTest, FindsTheStartOfThePlayerInTheOpenWithAFloorUnderIt)
  {
    constexpr float longest_fall = 4096.0f;
    std::size_t starts_found = 0;
    std::size_t starts_on_the_floor = 0;
    for (const auto &[name, file] : _levels)
    {
      BspCollision world;
      std::string error;
      ASSERT_TRUE(world.Build(file, 0, error)) << name << ": " << error;

      EntityText text;
      ASSERT_TRUE(text.Read(file.entities, error)) << name << ": " << error;
      const std::vector<std::pair<BspCollision, BspVector>> solids = MakeSolidEntities(file, text, name);

      for (const BspVector &start : FindStarts(text))
      {
        const std::string where = name + " at " + std::to_string(start.x) + " " + std::to_string(start.y) + " " +
          std::to_string(start.z);
        EXPECT_EQ(world.GetPointContents(start), BspContents::Empty) << where;

        // What the game does to put a monster on the floor, and what the
        // fall of a player comes to: from a unit above where it stands,
        // straight down, against the world and against every lift and
        // bridge that is an entity, of which the one that stops the move
        // first counts. The move is longer than the 256 units of the game:
        // a few levels let the player fall into them from high up.
        const BspVector from{start.x, start.y, start.z + 1.0f};
        const BspVector to{start.x, start.y, start.z - longest_fall};
        BspTraceResult result = world.TraceBox(nowhere, from, player_mins, player_maxs, to);
        for (const auto &[solid, origin] : solids)
        {
          const BspTraceResult other = solid.TraceBox(origin, from, player_mins, player_maxs, to);

          // a wall the start lies in is one of another kind of game, such
          // as a match, and is not there when this start is used
          if (!other.start_solid && other.fraction < result.fraction) { result = other; }
        }

        EXPECT_FALSE(result.all_solid) << where;
        EXPECT_FALSE(result.start_solid) << where;
        EXPECT_LT(result.fraction, 1.0f) << where;
        EXPECT_GT(result.plane_normal.z, 0.7f) << where;
        EXPECT_EQ(result.end_position.x, start.x) << where;
        EXPECT_EQ(result.end_position.y, start.y) << where;

        starts_found++;
        if (from.z - result.end_position.z < 64.0f) { starts_on_the_floor++; }
      }
    }
    EXPECT_GT(starts_found, 0u);

    // most starts stand on their floor or a little above it
    EXPECT_GE(starts_on_the_floor * 4, starts_found * 3) << starts_on_the_floor << " of " << starts_found;
  }

  TEST_F(BspCollisionRealDataTest, NeverEndsAMoveInWhatIsSolidUnlessItStartedThere)
  {
    constexpr auto solid = BspContents::Solid;
    constexpr int moves_for_each_hull = 2000;

    // the same moves every time
    std::mt19937 random(29);
    std::size_t moves_from_the_open = 0;
    std::size_t moves_stopped = 0;

    for (const auto &[name, file] : _levels)
    {
      BspCollision world;
      std::string error;
      ASSERT_TRUE(world.Build(file, 0, error)) << name << ": " << error;

      const BspModel &model = file.models[0];
      std::uniform_real_distribution<float> along_x(model.mins.x, model.maxs.x);
      std::uniform_real_distribution<float> along_y(model.mins.y, model.maxs.y);
      std::uniform_real_distribution<float> along_z(model.mins.z, model.maxs.z);

      // a long move across the level, and a short one as a step makes
      std::uniform_real_distribution<float> step(-48.0f, 48.0f);

      for (std::size_t number = 0; number < BspHull::hull_count; number++)
      {
        const BspHull &hull = world.GetHull(number);
        for (int i = 0; i < moves_for_each_hull; i++)
        {
          // most of the box around a level is solid, so a start is looked
          // for a few times until one lies in the open
          BspVector start{along_x(random), along_y(random), along_z(random)};
          for (int attempt = 0; attempt < 32 && hull.GetPointContents(start) == solid; attempt++)
          {
            start = {along_x(random), along_y(random), along_z(random)};
          }
          const BspVector end = i % 2 == 0
            ? BspVector{along_x(random), along_y(random), along_z(random)}
            : BspVector{start.x + step(random), start.y + step(random), start.z + step(random)};

          const BspTraceResult result = hull.TraceLine(start, end);
          const std::string where = name + ", hull " + std::to_string(number) + ", move " + std::to_string(i);

          EXPECT_GE(result.fraction, 0.0f) << where;
          EXPECT_LE(result.fraction, 1.0f) << where;
          EXPECT_EQ(result.start_solid, hull.GetPointContents(start) == solid) << where;
          if (result.all_solid) { EXPECT_TRUE(result.start_solid) << where; }
          if (!result.start_solid)
          {
            moves_from_the_open++;
            if (result.fraction < 1.0f) { moves_stopped++; }
            EXPECT_NE(hull.GetPointContents(result.end_position), solid) << where;
            EXPECT_TRUE(result.in_open || result.in_water) << where;
          }

          // the same move again gives the same
          const BspTraceResult again = hull.TraceLine(start, end);
          EXPECT_EQ(again.fraction, result.fraction) << where;
          EXPECT_EQ(again.end_position.z, result.end_position.z) << where;
        }
      }
    }

    // the moves did test something: many started in the open, and many of
    // those were stopped
    EXPECT_GT(moves_from_the_open, 1000u);
    EXPECT_GT(moves_stopped, 1000u);
  }
}
