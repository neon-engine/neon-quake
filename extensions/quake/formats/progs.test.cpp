#include "progs.hpp"

#include <bit>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "progs-builder.test.hpp"

namespace
{
  using quake::Progs;
  using quake::ProgsBuilder;
  using quake::ProgsOpcode;
  using quake::ProgsType;
  using ::testing::ElementsAre;
  using ::testing::HasSubstr;

  /// Where each number of the header is in the file, in bytes.
  constexpr std::size_t version_at = 0;
  constexpr std::size_t statements_offset_at = 8;
  constexpr std::size_t statements_count_at = 12;
  constexpr std::size_t global_definitions_count_at = 20;
  constexpr std::size_t field_definitions_offset_at = 24;
  constexpr std::size_t functions_count_at = 36;
  constexpr std::size_t strings_size_at = 44;
  constexpr std::size_t globals_count_at = 52;
  constexpr std::size_t entity_fields_at = 56;

  /// Writes a number of four bytes over what a file has at a place.
  void put_i32(std::vector<std::uint8_t> &bytes, const std::size_t at, const std::int32_t value)
  {
    const auto bits = static_cast<std::uint32_t>(value);
    for (std::size_t i = 0; i < 4; i++) { bytes[at + i] = static_cast<std::uint8_t>(bits >> (8 * i) & 0xff); }
  }

  /// A small program with one of everything.
  ProgsBuilder MakeProgram()
  {
    ProgsBuilder builder;
    builder.crc = 5927;
    const std::uint16_t speed = builder.Float(320.0f);
    builder.Name(speed, "speed", ProgsType::Float, true);
    const std::uint16_t greeting = builder.StringGlobal("hello");
    builder.Name(greeting, "greeting", ProgsType::String);
    builder.Field("origin", ProgsType::Vector);
    builder.Field("health", ProgsType::Float);

    const std::int32_t start = builder.Emit(ProgsOpcode::AddF, speed, speed, 1);
    builder.Emit(ProgsOpcode::Return, 1);
    builder.Function("main", start, speed, 1, {1});
    builder.Builtin("makevectors", 1);
    return builder;
  }

  /// Expects the bytes to be refused with an error that says `what`.
  void ExpectRefused(const std::vector<std::uint8_t> &bytes, const std::string &what)
  {
    Progs progs;
    std::string error;
    EXPECT_FALSE(progs.Read(bytes, error));
    EXPECT_THAT(error, HasSubstr(what));
  }

  TEST(ProgsTest, ReadsTheHeaderAndEveryTable)
  {
    const std::vector<std::uint8_t> bytes = MakeProgram().Build();
    Progs progs;
    std::string error;
    ASSERT_TRUE(progs.Read(bytes, error)) << error;

    EXPECT_EQ(progs.header.version, 6);
    EXPECT_EQ(progs.header.crc, 5927);
    EXPECT_EQ(progs.header.statements_offset, 60);
    EXPECT_EQ(progs.header.statements_count, 3);
    EXPECT_EQ(progs.header.global_definitions_count, 2);
    EXPECT_EQ(progs.header.field_definitions_count, 2);
    EXPECT_EQ(progs.header.functions_count, 3);
    EXPECT_EQ(progs.header.globals_count, 30);
    EXPECT_EQ(progs.header.entity_fields, 4);

    ASSERT_EQ(progs.statements.size(), 3u);
    EXPECT_EQ(progs.statements[0].opcode, static_cast<std::uint16_t>(ProgsOpcode::Done));
    EXPECT_EQ(progs.statements[1].opcode, static_cast<std::uint16_t>(ProgsOpcode::AddF));
    EXPECT_EQ(progs.statements[1].a, 28);
    EXPECT_EQ(progs.statements[1].b, 28);
    EXPECT_EQ(progs.statements[1].c, 1);
    EXPECT_EQ(progs.statements[2].opcode, static_cast<std::uint16_t>(ProgsOpcode::Return));

    ASSERT_EQ(progs.global_definitions.size(), 2u);
    EXPECT_EQ(progs.global_definitions[0].GetType(), ProgsType::Float);
    EXPECT_TRUE(progs.global_definitions[0].IsSaved());
    EXPECT_EQ(progs.global_definitions[0].offset, 28);
    EXPECT_EQ(progs.GetString(progs.global_definitions[0].name), "speed");
    EXPECT_EQ(progs.global_definitions[1].GetType(), ProgsType::String);
    EXPECT_FALSE(progs.global_definitions[1].IsSaved());

    ASSERT_EQ(progs.field_definitions.size(), 2u);
    EXPECT_EQ(progs.field_definitions[0].GetType(), ProgsType::Vector);
    EXPECT_EQ(progs.field_definitions[0].offset, 0);
    EXPECT_EQ(progs.field_definitions[1].GetType(), ProgsType::Float);
    EXPECT_EQ(progs.field_definitions[1].offset, 3);
    EXPECT_EQ(progs.GetString(progs.field_definitions[1].name), "health");

    ASSERT_EQ(progs.functions.size(), 3u);
    EXPECT_EQ(progs.functions[1].first_statement, 1);
    EXPECT_EQ(progs.functions[1].first_local, 28);
    EXPECT_EQ(progs.functions[1].locals_count, 1);
    EXPECT_EQ(progs.functions[1].parameters_count, 1);
    EXPECT_THAT(progs.functions[1].parameter_sizes, ElementsAre(1, 0, 0, 0, 0, 0, 0, 0));
    EXPECT_EQ(progs.GetString(progs.functions[1].name), "main");
    EXPECT_EQ(progs.GetString(progs.functions[1].file), "test.qc");
    EXPECT_FALSE(progs.functions[1].IsBuiltin());
    EXPECT_EQ(progs.functions[2].first_statement, -1);
    EXPECT_TRUE(progs.functions[2].IsBuiltin());

    ASSERT_EQ(progs.globals.size(), 30u);
    EXPECT_EQ(std::bit_cast<float>(progs.globals[28]), 320.0f);
    EXPECT_EQ(progs.GetString(static_cast<std::int32_t>(progs.globals[29])), "hello");
  }

  TEST(ProgsTest, FindsFunctionsGlobalsAndFieldsByName)
  {
    Progs progs;
    std::string error;
    ASSERT_TRUE(progs.Read(MakeProgram().Build(), error)) << error;

    EXPECT_EQ(progs.FindFunction("main"), 1);
    EXPECT_EQ(progs.FindFunction("makevectors"), 2);
    EXPECT_EQ(progs.FindFunction("missing"), std::nullopt);

    ASSERT_NE(progs.FindGlobal("greeting"), nullptr);
    EXPECT_EQ(progs.FindGlobal("greeting")->offset, 29);
    EXPECT_EQ(progs.FindGlobal("health"), nullptr);

    ASSERT_NE(progs.FindField("health"), nullptr);
    EXPECT_EQ(progs.FindField("health")->offset, 3);
    EXPECT_EQ(progs.FindField("speed"), nullptr);
  }

  TEST(ProgsTest, GivesNothingForAStringOutsideTheStrings)
  {
    Progs progs;
    std::string error;
    ASSERT_TRUE(progs.Read(MakeProgram().Build(), error)) << error;

    EXPECT_TRUE(progs.HasString(0));
    EXPECT_EQ(progs.GetString(0), "");
    EXPECT_FALSE(progs.HasString(-1));
    EXPECT_EQ(progs.GetString(-1), "");
    EXPECT_FALSE(progs.HasString(static_cast<std::int32_t>(progs.strings.size())));
    EXPECT_EQ(progs.GetString(1 << 30), "");
  }

  TEST(ProgsTest, RefusesAFileShorterThanAHeader)
  {
    std::vector<std::uint8_t> bytes = MakeProgram().Build();
    bytes.resize(59);
    ExpectRefused(bytes, "fewer than the 60 of a header");
    ExpectRefused({}, "fewer than the 60 of a header");
  }

  TEST(ProgsTest, RefusesAVersionThatIsNotSix)
  {
    std::vector<std::uint8_t> bytes = MakeProgram().Build();
    put_i32(bytes, version_at, 7);
    ExpectRefused(bytes, "version 7");
  }

  TEST(ProgsTest, RefusesATableThatEndsOutsideTheFile)
  {
    const std::vector<std::uint8_t> good = MakeProgram().Build();

    std::vector<std::uint8_t> bytes = good;
    put_i32(bytes, statements_count_at, 1000);
    ExpectRefused(bytes, "table of statements");

    bytes = good;
    put_i32(bytes, global_definitions_count_at, 1000);
    ExpectRefused(bytes, "table of global definitions");

    bytes = good;
    put_i32(bytes, field_definitions_offset_at, static_cast<std::int32_t>(good.size()) - 8);
    ExpectRefused(bytes, "table of field definitions");

    bytes = good;
    put_i32(bytes, functions_count_at, 1000);
    ExpectRefused(bytes, "table of functions");

    bytes = good;
    put_i32(bytes, strings_size_at, 100000);
    ExpectRefused(bytes, "table of strings");

    // the last table, cut short by one byte
    bytes = good;
    bytes.pop_back();
    ExpectRefused(bytes, "table of globals");
  }

  TEST(ProgsTest, RefusesANegativeOffsetOrCount)
  {
    const std::vector<std::uint8_t> good = MakeProgram().Build();

    std::vector<std::uint8_t> bytes = good;
    put_i32(bytes, statements_offset_at, -8);
    ExpectRefused(bytes, "negative");

    bytes = good;
    put_i32(bytes, functions_count_at, -1);
    ExpectRefused(bytes, "negative");

    bytes = good;
    put_i32(bytes, entity_fields_at, -1);
    ExpectRefused(bytes, "-1 fields");
  }

  TEST(ProgsTest, RefusesACountSoLargeThatItsBytesWouldWrapAround)
  {
    std::vector<std::uint8_t> bytes = MakeProgram().Build();
    put_i32(bytes, functions_count_at, 0x7fffffff);
    ExpectRefused(bytes, "table of functions");
  }

  TEST(ProgsTest, RefusesFewerGlobalsThanTheReservedOnes)
  {
    std::vector<std::uint8_t> bytes = ProgsBuilder().Build();
    put_i32(bytes, globals_count_at, 27);
    ExpectRefused(bytes, "27 globals");
  }

  TEST(ProgsTest, RefusesStringsThatDoNotEndWithAZero)
  {
    ProgsBuilder builder = MakeProgram();
    builder.strings.push_back('x');
    ExpectRefused(builder.Build(), "do not end with a zero");
  }

  TEST(ProgsTest, RefusesADefinitionWhoseNameIsOutsideTheStrings)
  {
    ProgsBuilder global = MakeProgram();
    global.global_definitions[1].name = 100000;
    ExpectRefused(global.Build(), "name of global definition 1");

    ProgsBuilder field = MakeProgram();
    field.field_definitions[0].name = -1;
    ExpectRefused(field.Build(), "name of field definition 0");
  }

  TEST(ProgsTest, RefusesADefinitionThatPointsOutsideItsTable)
  {
    ProgsBuilder global = MakeProgram();
    global.global_definitions[0].offset = 30;
    ExpectRefused(global.Build(), "Global definition 0 (speed) is at offset 30");

    ProgsBuilder field = MakeProgram();
    field.field_definitions[1].offset = 4;
    ExpectRefused(field.Build(), "Field definition 1 (health) is at offset 4");
  }

  TEST(ProgsTest, RefusesAFunctionThatPointsOutsideATable)
  {
    ProgsBuilder name = MakeProgram();
    name.functions[1].name = 100000;
    ExpectRefused(name.Build(), "name or the file of function 1");

    ProgsBuilder file = MakeProgram();
    file.functions[1].file = -5;
    ExpectRefused(file.Build(), "name or the file of function 1");

    ProgsBuilder statement = MakeProgram();
    statement.functions[1].first_statement = 3;
    ExpectRefused(statement.Build(), "Function 1 (main) starts at statement 3");

    ProgsBuilder locals = MakeProgram();
    locals.functions[1].locals_count = 3;
    ExpectRefused(locals.Build(), "Function 1 (main) has 3 locals from offset 28");

    ProgsBuilder negative = MakeProgram();
    negative.functions[1].first_local = -1;
    ExpectRefused(negative.Build(), "Function 1 (main) has 1 locals from offset -1");
  }

  TEST(ProgsTest, RefusesAFunctionWithParametersItCannotKeep)
  {
    ProgsBuilder many = MakeProgram();
    many.functions[1].parameters_count = 9;
    ExpectRefused(many.Build(), "takes 9 parameters");

    // `main` keeps its locals from offset 28 of 30 globals
    ProgsBuilder large = MakeProgram();
    large.functions[1].parameter_sizes[0] = 3;
    ExpectRefused(large.Build(), "parameters of 3 cells from offset 28, outside the 30 globals");
  }

  TEST(ProgsTest, AcceptsAFunctionWithParametersAndNoLocalsAsIdsQccWroteIt)
  {
    // as SUB_AttackFinished is in id's progs.dat: declared ahead of its
    // body, so its one parameter is not counted among its locals
    ProgsBuilder builder = MakeProgram();
    builder.functions[1].locals_count = 0;
    const std::vector<std::uint8_t> bytes = builder.Build();

    Progs progs;
    std::string error;
    ASSERT_TRUE(progs.Read(bytes, error)) << error;
    EXPECT_EQ(progs.functions[1].locals_count, 0);
    EXPECT_EQ(progs.functions[1].parameters_count, 1);
  }

  TEST(ProgsTest, AcceptsABuiltinThatTakesAnyNumberOfParameters)
  {
    ProgsBuilder builder = MakeProgram();
    builder.functions[2].parameters_count = -1;

    Progs progs;
    std::string error;
    EXPECT_TRUE(progs.Read(builder.Build(), error)) << error;
  }

  TEST(ProgsTest, StaysAsItWasWhenItRefuses)
  {
    Progs progs;
    std::string error;
    ASSERT_TRUE(progs.Read(MakeProgram().Build(), error)) << error;

    std::vector<std::uint8_t> bytes = MakeProgram().Build();
    put_i32(bytes, version_at, 0);
    EXPECT_FALSE(progs.Read(bytes, error));
    EXPECT_EQ(progs.header.version, 6);
    EXPECT_EQ(progs.functions.size(), 3u);
  }
}
