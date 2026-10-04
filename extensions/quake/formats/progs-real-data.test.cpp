#include "progs.hpp"

#include <cstdint>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <string_view>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "game/qc-builtin-number.hpp"
#include "progs-opcode.hpp"
#include "real-data.test.hpp"

// The tests of Progs with the game code of a real game, which a real
// compiler made: the `progs.dat` of the pak the tests are given.
namespace
{
  using quake::Progs;
  using quake::ProgsDefinition;
  using quake::ProgsFunction;
  using quake::ProgsOpcode;
  using quake::ProgsStatement;
  using quake::ProgsType;
  using quake::RealData;

  /// Reads the game code of the real game once for all tests, which skip
  /// themselves when the data is not there.
  class ProgsRealDataTest : public ::testing::Test
  {
  protected:
    Progs _progs;

    void SetUp() override
    {
      const RealData &data = RealData::Get();
      if (!data.IsThere()) { GTEST_SKIP() << data.GetProblem(); }

      const auto bytes = data.GetBytes("progs.dat");
      ASSERT_FALSE(bytes.empty()) << "The pak has no progs.dat. " << data.GetProblem();

      std::string error;
      ASSERT_TRUE(_progs.Read(bytes, error)) << error;
    }
  };

  TEST_F(ProgsRealDataTest, ReadsTheFileOfTheVersionOfTheOriginalWithSensibleCounts)
  {
    EXPECT_EQ(_progs.header.version, Progs::version);

    // the game code of a whole game: thousands of statements and functions,
    // and more globals than the reserved ones and those of the engine
    EXPECT_GT(_progs.statements.size(), 10000u);
    EXPECT_GT(_progs.functions.size(), 1000u);
    EXPECT_GT(_progs.global_definitions.size(), 1000u);
    EXPECT_GT(_progs.field_definitions.size(), 100u);
    EXPECT_GT(_progs.globals.size(), 1000u);
    EXPECT_GT(_progs.strings.size(), 10000u);
    EXPECT_GT(_progs.header.entity_fields, 100);

    // an operand is 16 bits, so a compiler of the original's format cannot
    // name more globals than these
    EXPECT_LE(_progs.globals.size(), 65536u);

    // the first of each table is nothing, as the original expects
    EXPECT_EQ(_progs.GetString(0), "");
    EXPECT_EQ(_progs.statements[0].opcode, static_cast<std::uint16_t>(ProgsOpcode::Done));
    EXPECT_EQ(_progs.functions[0].first_statement, 0);
  }

  TEST_F(ProgsRealDataTest, HasTheFunctionsTheEngineCallsByName)
  {
    for (const std::string_view name : {
           "main", "StartFrame", "PlayerPreThink", "PlayerPostThink", "ClientConnect", "ClientDisconnect",
           "PutClientInServer", "ClientKill", "SetNewParms", "SetChangeParms", "worldspawn",
         })
    {
      const auto function = _progs.FindFunction(name);
      ASSERT_TRUE(function.has_value()) << name;
      EXPECT_FALSE(_progs.functions[static_cast<std::size_t>(*function)].IsBuiltin()) << name;
      EXPECT_GT(_progs.functions[static_cast<std::size_t>(*function)].first_statement, 0) << name;
    }
  }

  TEST_F(ProgsRealDataTest, HasTheGlobalsAndFieldsTheEngineReliesOn)
  {
    // the engine of the original has these at fixed places, in this order
    // from the first global after the reserved ones
    const ProgsDefinition *self = _progs.FindGlobal("self");
    const ProgsDefinition *other = _progs.FindGlobal("other");
    const ProgsDefinition *world = _progs.FindGlobal("world");
    const ProgsDefinition *time = _progs.FindGlobal("time");
    const ProgsDefinition *frame_time = _progs.FindGlobal("frametime");
    ASSERT_NE(self, nullptr);
    ASSERT_NE(other, nullptr);
    ASSERT_NE(world, nullptr);
    ASSERT_NE(time, nullptr);
    ASSERT_NE(frame_time, nullptr);
    EXPECT_EQ(self->offset, Progs::reserved_globals);
    EXPECT_EQ(other->offset, Progs::reserved_globals + 1);
    EXPECT_EQ(world->offset, Progs::reserved_globals + 2);
    EXPECT_EQ(time->offset, Progs::reserved_globals + 3);
    EXPECT_EQ(frame_time->offset, Progs::reserved_globals + 4);
    EXPECT_EQ(self->GetType(), ProgsType::Entity);
    EXPECT_EQ(other->GetType(), ProgsType::Entity);
    EXPECT_EQ(world->GetType(), ProgsType::Entity);
    EXPECT_EQ(time->GetType(), ProgsType::Float);
    EXPECT_EQ(frame_time->GetType(), ProgsType::Float);

    const std::map<std::string_view, ProgsType> fields = {
      {"origin", ProgsType::Vector},
      {"angles", ProgsType::Vector},
      {"classname", ProgsType::String},
      {"model", ProgsType::String},
      {"think", ProgsType::Function},
      {"nextthink", ProgsType::Float},
      {"solid", ProgsType::Float},
      {"movetype", ProgsType::Float},
      {"health", ProgsType::Float},
    };
    for (const auto &[name, type] : fields)
    {
      const ProgsDefinition *field = _progs.FindField(name);
      ASSERT_NE(field, nullptr) << name;
      EXPECT_EQ(field->GetType(), type) << name;
    }
  }

  TEST_F(ProgsRealDataTest, HasOnlyOpcodesTheMachineKnows)
  {
    std::map<std::uint16_t, std::size_t> unknown;
    for (const ProgsStatement &statement : _progs.statements)
    {
      if (statement.opcode > static_cast<std::uint16_t>(ProgsOpcode::BitOr)) { unknown[statement.opcode]++; }
    }
    EXPECT_TRUE(unknown.empty()) << "The first opcode that is not known is " << unknown.begin()->first << ", in "
                                 << unknown.begin()->second << " statements";
  }

  TEST_F(ProgsRealDataTest, HasEveryFunctionAndStringGlobalInsideItsTable)
  {
    // what the loader does not check, since only a definition says what a
    // global holds: a global that is a function names one of the functions,
    // and one that is a string a string
    for (const ProgsDefinition &definition : _progs.global_definitions)
    {
      const std::int32_t value = static_cast<std::int32_t>(_progs.globals[definition.offset]);
      const std::string_view name = _progs.GetString(definition.name);
      if (definition.GetType() == ProgsType::Function)
      {
        EXPECT_GE(value, 0) << name;
        EXPECT_LT(static_cast<std::size_t>(value), _progs.functions.size()) << name;
      }
      if (definition.GetType() == ProgsType::String) { EXPECT_TRUE(_progs.HasString(value)) << name; }
    }
  }

  TEST_F(ProgsRealDataTest, ReadsTheGameCodeForQuakeWorldToo)
  {
    // the same game compiled for the other engine of the original, which
    // the machine does not run: the format is the same
    const auto bytes = RealData::Get().GetBytes("qwprogs.dat");
    if (bytes.empty()) { GTEST_SKIP() << "The pak has no qwprogs.dat"; }

    Progs progs;
    std::string error;
    ASSERT_TRUE(progs.Read(bytes, error)) << error;
    EXPECT_NE(progs.header.crc, _progs.header.crc);
    EXPECT_TRUE(progs.FindFunction("worldspawn").has_value());
    for (const ProgsStatement &statement : progs.statements)
    {
      ASSERT_LE(statement.opcode, static_cast<std::uint16_t>(ProgsOpcode::BitOr));
    }
  }

  // The builtins the game code of LibreQuake 0.9 needs, which is what has to
  // be mapped onto the engine. The compiler left their names out of the
  // file, so the names are those of the original for the numbers:
  //
  //   1 makevectors     2 setorigin        3 setmodel        4 setsize
  //   6 break           7 random           8 sound           9 normalize
  //  10 error          11 objerror        12 vlen           13 vectoyaw
  //  14 spawn          15 remove          16 traceline      17 checkclient
  //  18 find           19 precache_sound  20 precache_model 21 stuffcmd
  //  22 findradius     23 bprint          24 sprint         25 dprint
  //  26 ftos           27 vtos            28 coredump       29 traceon
  //  30 traceoff       31 eprint          32 walkmove       34 droptofloor
  //  35 lightstyle     36 rint            37 floor          38 ceil
  //  40 checkbottom    41 pointcontents   43 fabs           44 aim
  //  45 cvar           46 localcmd        47 nextent        48 particle
  //  49 ChangeYaw      51 vectoangles     52 WriteByte      53 WriteChar
  //  54 WriteShort     55 WriteLong       56 WriteCoord     57 WriteAngle
  //  58 WriteString    59 WriteEntity     67 movetogoal     68 precache_file
  //  69 makestatic     70 changelevel     72 cvar_set       73 centerprint
  //  74 ambientsound   75 precache_model2 76 precache_sound2
  //  77 precache_file2 78 setspawnparms
  TEST_F(ProgsRealDataTest, NeedsOnlyBuiltinsOfTheOriginal)
  {
    // the number of each builtin, with the numbers of parameters the
    // functions that stand for it take: several functions may be one builtin
    std::map<std::int64_t, std::set<std::int32_t>> builtins;
    std::size_t named = 0;
    for (const ProgsFunction &function : _progs.functions)
    {
      if (!function.IsBuiltin()) { continue; }

      builtins[-static_cast<std::int64_t>(function.first_statement)].insert(function.parameters_count);
      if (!_progs.GetString(function.name).empty()) { named++; }
    }

    std::cout << "The builtins the game code needs, " << builtins.size() << " of them, " << named
              << " with a name in the file:\n";
    for (const auto &[number, parameter_counts] : builtins)
    {
      const std::string_view name = quake::QcBuiltinName(static_cast<std::int32_t>(number));
      EXPECT_FALSE(name.empty()) << "Builtin " << number << " is none of the original";

      std::cout << "  " << number << " " << name << ", parameters:";
      for (const std::int32_t count : parameter_counts) { std::cout << " " << count; }
      std::cout << "\n";
    }
    EXPECT_GT(builtins.size(), 50u);
  }
}
