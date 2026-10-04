#include "bsp-light-point.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
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
  using quake::BspEntity;
  using quake::BspFile;
  using quake::BspLightFilter;
  using quake::BspLightPoint;
  using quake::BspLightSample;
  using quake::BspModel;
  using quake::BspVector;
  using quake::EntityText;
  using quake::RealData;

  /// The levels of the real game that are read, each with its name: the
  /// ones that are played, and the small ones that are boxes of ammunition
  /// and health. A level of another version than the original's is refused
  /// by the reader and left out.
  class BspLightPointRealDataTest : public ::testing::Test
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

  /// The places of the things of a level that the game draws as models and
  /// lights by the floor: the origins of its items, weapons, and monsters.
  std::vector<BspVector> FindThings(const BspFile &file, const std::string &name)
  {
    EntityText text;
    std::string error;
    EXPECT_TRUE(text.Read(file.entities, error)) << name << ": " << error;

    std::vector<BspVector> places;
    for (const BspEntity &entity : text.entities)
    {
      const std::string *class_name = entity.Find("classname");
      const std::string *origin = entity.Find("origin");
      if (class_name == nullptr || origin == nullptr) { continue; }
      if (!class_name->starts_with("item_") && !class_name->starts_with("weapon_") &&
        !class_name->starts_with("monster_"))
      {
        continue;
      }

      BspVector place;
      std::istringstream numbers(*origin);
      numbers >> place.x >> place.y >> place.z;
      EXPECT_FALSE(numbers.fail()) << name << ": " << *origin;
      places.push_back(place);
    }
    return places;
  }

  TEST_F(BspLightPointRealDataTest, IsMadeOfEveryLevelTheReaderTakes)
  {
    for (const auto &[name, file] : _levels)
    {
      BspLightPoint light;
      std::string error;
      EXPECT_TRUE(light.Build(file, error)) << name << ": " << error;

      // and with light in colours, as a file of them next to the level has it
      const std::vector<std::uint8_t> coloured(file.lighting.size() * 3, 100);
      EXPECT_TRUE(light.Build(file, error, coloured)) << name << ": " << error;
    }
  }

  TEST_F(BspLightPointRealDataTest, FindsTheFloorUnderNearlyEveryItemAndMonster)
  {
    std::size_t things = 0;
    std::size_t found = 0;
    std::size_t dark = 0;
    float darkest = 1.0e9f;
    float brightest = 0.0f;
    double sum = 0.0;

    for (const auto &[name, file] : _levels)
    {
      BspLightPoint light;
      std::string error;
      ASSERT_TRUE(light.Build(file, error)) << name << ": " << error;
      if (!light.HasLight()) { continue; }

      std::size_t found_here = 0;
      const std::vector<BspVector> places = FindThings(file, name);
      for (const BspVector &place : places)
      {
        const BspLightSample nearest = light.Sample(place);
        const BspLightSample blended = light.Sample(place, {}, BspLightFilter::Bilinear);
        EXPECT_EQ(nearest.is_found, blended.is_found) << name;
        if (!nearest.is_found) { continue; }

        // both ways find the same face, at or below the thing, and blending
        // stays between the samples it blends
        EXPECT_EQ(nearest.face, blended.face) << name;
        EXPECT_LE(nearest.place.z, place.z) << name;
        EXPECT_EQ(nearest.red, nearest.green) << name;
        EXPECT_GE(blended.red, 0.0f) << name;
        EXPECT_LE(blended.red, 4.0f * 255.0f) << name;

        found_here++;
        if (nearest.red == 0.0f) { dark++; }
        darkest = std::min(darkest, nearest.red);
        brightest = std::max(brightest, nearest.red);
        sum += nearest.red;
      }

      things += places.size();
      found += found_here;
      if (found_here != places.size())
      {
        std::cout << name << ": no floor under " << places.size() - found_here << " of " <<
          places.size() << " things\n";
      }
    }

    if (things == 0) { GTEST_SKIP() << "No level with items or monsters in the paks"; }
    std::cout << "A floor under " << found << " of " << things << " items, weapons, and monsters; " <<
      "its light from " << darkest << " to " << brightest << ", " << sum / static_cast<double>(found) <<
      " on average, and dark under " << dark << "\n";
    EXPECT_GE(static_cast<double>(found), 0.99 * static_cast<double>(things));
  }

  TEST_F(BspLightPointRealDataTest, GivesALightForAnyPlaceInAndAroundEveryLevel)
  {
    // every style at another brightness, so that all of them are read
    std::vector<float> styles(BspLightPoint::style_count);
    for (std::size_t i = 0; i < styles.size(); i++) { styles[i] = 0.5f + static_cast<float>(i % 4) * 0.5f; }

    std::mt19937 random(29);
    for (const auto &[name, file] : _levels)
    {
      BspLightPoint light;
      std::string error;
      ASSERT_TRUE(light.Build(file, error)) << name << ": " << error;

      // the box of the world and a little around it
      const BspModel &world = file.models[0];
      std::uniform_real_distribution<float> x(world.mins.x - 64.0f, world.maxs.x + 64.0f);
      std::uniform_real_distribution<float> y(world.mins.y - 64.0f, world.maxs.y + 64.0f);
      std::uniform_real_distribution<float> z(world.mins.z - 64.0f, world.maxs.z + 64.0f);

      for (int i = 0; i < 2000; i++)
      {
        const BspVector place{x(random), y(random), z(random)};
        const BspLightFilter filter = i % 2 == 0 ? BspLightFilter::Nearest : BspLightFilter::Bilinear;
        const BspLightSample sample = light.Sample(place, styles, filter);

        if (!light.HasLight())
        {
          EXPECT_FALSE(sample.is_found) << name;
          EXPECT_EQ(sample.red, BspLightPoint::fully_bright) << name;
          continue;
        }
        if (!sample.is_found)
        {
          EXPECT_EQ(sample.red, 0.0f) << name;
          continue;
        }

        // four styles at most, none brighter than twice 255
        ASSERT_LT(sample.face, file.faces.size()) << name;
        EXPECT_TRUE(std::isfinite(sample.red)) << name;
        EXPECT_GE(sample.red, 0.0f) << name;
        EXPECT_LE(sample.red, 4.0f * 2.0f * 255.0f) << name;
        EXPECT_LE(sample.place.z, place.z) << name;
        EXPECT_FLOAT_EQ(sample.place.x, place.x) << name;
        EXPECT_FLOAT_EQ(sample.place.y, place.y) << name;
      }
    }
  }
} // namespace
