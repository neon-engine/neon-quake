#include "player-stats.hpp"

#include <limits>
#include <string>
#include <string_view>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "formats/real-data.test.hpp"
#include "level-program.test.hpp"
#include "qc-item.hpp"

namespace
{
  using quake::HasItem;
  using quake::LevelProgram;
  using quake::PlayerStats;
  using quake::Progs;
  using quake::ProgsType;
  using quake::QcFields;
  using quake::QcGlobals;
  using quake::QcItem;
  using quake::QcMachine;
  using quake::RealData;

  /// A program with everything the stats are read from.
  LevelProgram make_program()
  {
    LevelProgram program;
    for (const std::string_view name : {
           "armorvalue", "armortype", "currentammo", "ammo_shells", "ammo_nails", "ammo_rockets", "ammo_cells",
           "weapon", "items",
         })
    {
      program.builder.Field(name, ProgsType::Float);
    }
    program.builder.Field("message", ProgsType::String);
    for (const std::string_view name : {"total_secrets", "total_monsters", "found_secrets", "killed_monsters"})
    {
      program.builder.Name(program.builder.Float(), name, ProgsType::Float);
    }
    return program;
  }

  TEST(PlayerStatsTest, ReadsWhatAPlayerHasAndWhatTheLevelCounts)
  {
    const LevelProgram program = make_program();
    QcMachine machine = program.Make();
    const QcFields fields(machine.GetProgs());
    const QcGlobals globals(machine.GetProgs());
    const std::int32_t player = machine.CreateEntity().value_or(-1);
    ASSERT_EQ(player, 1);

    fields.health.Set(machine, player, 87.0f);
    fields.armorvalue.Set(machine, player, 150.0f);
    fields.armortype.Set(machine, player, 0.6f);
    fields.currentammo.Set(machine, player, 42.0f);
    fields.ammo_shells.Set(machine, player, 42.0f);
    fields.ammo_nails.Set(machine, player, 120.0f);
    fields.ammo_rockets.Set(machine, player, 5.0f);
    fields.ammo_cells.Set(machine, player, 13.0f);
    fields.weapon.Set(machine, player, 2.0f);
    fields.items.Set(machine, player, 1.0f + 2.0f + 256.0f + 4096.0f + 16384.0f + 4194304.0f);
    fields.message.SetText(machine, 0, "the Slipgate Complex");

    globals.killed_monsters.Set(machine, 12.0f);
    globals.total_monsters.Set(machine, 34.0f);
    globals.found_secrets.Set(machine, 1.0f);
    globals.total_secrets.Set(machine, 6.0f);
    globals.mapname.SetText(machine, "e1m1");
    globals.time.Set(machine, 307.5f);

    const PlayerStats stats = PlayerStats::Read(machine, fields, globals, player);
    EXPECT_EQ(stats.health, 87);
    EXPECT_EQ(stats.armor, 150);
    EXPECT_FLOAT_EQ(stats.armor_type, 0.6f);
    EXPECT_EQ(stats.ammo, 42);
    EXPECT_EQ(stats.shells, 42);
    EXPECT_EQ(stats.nails, 120);
    EXPECT_EQ(stats.rockets, 5);
    EXPECT_EQ(stats.cells, 13);
    EXPECT_EQ(stats.weapon, static_cast<std::uint32_t>(QcItem::SuperShotgun));
    EXPECT_EQ(stats.items, 1u + 2u + 256u + 4096u + 16384u + 4194304u);
    EXPECT_TRUE(HasItem(stats.items, QcItem::Quad));
    EXPECT_TRUE(HasItem(stats.items, QcItem::Armor2));
    EXPECT_FALSE(HasItem(stats.items, QcItem::Sigil1));
    EXPECT_EQ(stats.killed_monsters, 12);
    EXPECT_EQ(stats.total_monsters, 34);
    EXPECT_EQ(stats.found_secrets, 1);
    EXPECT_EQ(stats.total_secrets, 6);
    EXPECT_EQ(stats.level_name, "the Slipgate Complex");
    EXPECT_EQ(stats.map_name, "e1m1");
    EXPECT_EQ(stats.time, 307.5f);
  }

  TEST(PlayerStatsTest, PutsTheRunesOfTheServerFlagsOnTopOfTheItems)
  {
    const LevelProgram program = make_program();
    QcMachine machine = program.Make();
    const QcFields fields(machine.GetProgs());
    const QcGlobals globals(machine.GetProgs());
    const std::int32_t player = machine.CreateEntity().value_or(-1);

    fields.items.Set(machine, player, 4096.0f);

    // the first and the last rune, and a flag above the four that is none
    globals.serverflags.Set(machine, 1.0f + 8.0f + 16.0f);

    const PlayerStats stats = PlayerStats::Read(machine, fields, globals, player);
    EXPECT_TRUE(HasItem(stats.items, QcItem::Axe));
    EXPECT_TRUE(HasItem(stats.items, QcItem::Sigil1));
    EXPECT_FALSE(HasItem(stats.items, QcItem::Sigil2));
    EXPECT_FALSE(HasItem(stats.items, QcItem::Sigil3));
    EXPECT_TRUE(HasItem(stats.items, QcItem::Sigil4));
    EXPECT_EQ(stats.items, 4096u | (9u << 28));
  }

  TEST(PlayerStatsTest, CutsAFloatToAWholeNumberAndReadsWhatIsNoNumberAsZero)
  {
    const LevelProgram program = make_program();
    QcMachine machine = program.Make();
    const QcFields fields(machine.GetProgs());
    const QcGlobals globals(machine.GetProgs());
    const std::int32_t player = machine.CreateEntity().value_or(-1);

    // a health under one is none, as the original shows it
    fields.health.Set(machine, player, 0.5f);
    fields.armorvalue.Set(machine, player, 99.9f);
    fields.currentammo.Set(machine, player, -0.5f);
    fields.ammo_shells.Set(machine, player, std::numeric_limits<float>::quiet_NaN());
    fields.ammo_nails.Set(machine, player, std::numeric_limits<float>::infinity());
    fields.ammo_rockets.Set(machine, player, 1.0e20f);
    fields.ammo_cells.Set(machine, player, -1.0e20f);

    const PlayerStats stats = PlayerStats::Read(machine, fields, globals, player);
    EXPECT_EQ(stats.health, 0);
    EXPECT_EQ(stats.armor, 99);
    EXPECT_EQ(stats.ammo, 0);
    EXPECT_EQ(stats.shells, 0);
    EXPECT_EQ(stats.nails, 0);
    EXPECT_EQ(stats.rockets, 0);
    EXPECT_EQ(stats.cells, 0);
  }

  TEST(PlayerStatsTest, ReadsNothingFromAProgramThatLacksItAllAndForAPlayerThatThereIsNot)
  {
    // the program of the tests has a health and little else
    const LevelProgram bare;
    QcMachine machine = bare.Make();
    const QcFields fields(machine.GetProgs());
    const QcGlobals globals(machine.GetProgs());
    const std::int32_t player = machine.CreateEntity().value_or(-1);
    fields.health.Set(machine, player, 50.0f);

    PlayerStats stats = PlayerStats::Read(machine, fields, globals, player);
    EXPECT_EQ(stats.health, 50);
    EXPECT_EQ(stats.armor, 0);
    EXPECT_EQ(stats.items, 0u);
    EXPECT_EQ(stats.total_monsters, 0);
    EXPECT_EQ(stats.level_name, "");

    stats = PlayerStats::Read(machine, fields, globals, 77);
    EXPECT_EQ(stats.health, 0);
    stats = PlayerStats::Read(machine, fields, globals, -1);
    EXPECT_EQ(stats.health, 0);
  }

  TEST(PlayerStatsTest, FindsAllItReadsInTheGameCodeOfRealData)
  {
    const RealData &data = RealData::Get();
    if (!data.IsThere()) { GTEST_SKIP() << data.GetProblem(); }

    Progs progs;
    std::string error;
    ASSERT_TRUE(progs.Read(data.GetBytes("progs.dat"), error)) << error;

    const QcFields fields(progs);
    EXPECT_TRUE(fields.health.IsFound());
    EXPECT_TRUE(fields.armorvalue.IsFound());
    EXPECT_TRUE(fields.armortype.IsFound());
    EXPECT_TRUE(fields.currentammo.IsFound());
    EXPECT_TRUE(fields.ammo_shells.IsFound());
    EXPECT_TRUE(fields.ammo_nails.IsFound());
    EXPECT_TRUE(fields.ammo_rockets.IsFound());
    EXPECT_TRUE(fields.ammo_cells.IsFound());
    EXPECT_TRUE(fields.weapon.IsFound());
    EXPECT_TRUE(fields.items.IsFound());
    EXPECT_TRUE(fields.message.IsFound());

    const QcGlobals globals(progs);
    EXPECT_TRUE(globals.serverflags.IsFound());
    EXPECT_TRUE(globals.total_monsters.IsFound());
    EXPECT_TRUE(globals.killed_monsters.IsFound());
    EXPECT_TRUE(globals.total_secrets.IsFound());
    EXPECT_TRUE(globals.found_secrets.IsFound());
    EXPECT_TRUE(globals.mapname.IsFound());
    EXPECT_TRUE(globals.time.IsFound());
  }
}
