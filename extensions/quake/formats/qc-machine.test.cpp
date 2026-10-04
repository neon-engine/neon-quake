#include "qc-machine.hpp"

#include <array>
#include <cmath>
#include <span>
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
  using quake::QcCell;
  using quake::QcLimits;
  using quake::QcMachine;
  using ::testing::ElementsAre;
  using ::testing::HasSubstr;

  /// A machine with the program a builder holds, read as a file is.
  QcMachine MakeMachine(const ProgsBuilder &builder, const QcLimits &limits = {})
  {
    Progs progs;
    std::string error;
    EXPECT_TRUE(progs.Read(builder.Build(), error)) << error;
    return QcMachine(std::move(progs), limits);
  }

  /// Ends the statements a test has added since the builder was made and
  /// makes them the function `main`.
  void EndMain(ProgsBuilder &builder)
  {
    builder.Emit(ProgsOpcode::Done);
    builder.Function("main", 1);
  }

  /// A machine that has run those statements to their end.
  QcMachine RunMain(ProgsBuilder &builder)
  {
    EndMain(builder);
    QcMachine machine = MakeMachine(builder);
    EXPECT_TRUE(machine.Call("main")) << machine.GetError().message;
    EXPECT_FALSE(machine.HasFailed());
    return machine;
  }

  TEST(QcMachineTest, StartsWithTheGlobalsOfTheFile)
  {
    ProgsBuilder builder;
    const std::uint16_t speed = builder.Float(320.0f);
    const std::uint16_t count = builder.Integer(-3);
    const std::uint16_t up = builder.Vector(0.0f, 0.0f, 1.0f);
    QcMachine machine = MakeMachine(builder);

    EXPECT_EQ(machine.GetGlobalCount(), 33);
    EXPECT_EQ(machine.GetFloat(speed), 320.0f);
    EXPECT_EQ(machine.GetInteger(count), -3);
    EXPECT_THAT(machine.GetVector(up), ElementsAre(0.0f, 0.0f, 1.0f));
  }

  TEST(QcMachineTest, WritesGlobalsAndRefusesThoseOutsideThem)
  {
    ProgsBuilder builder;
    const std::uint16_t place = builder.Vector();
    QcMachine machine = MakeMachine(builder);

    EXPECT_TRUE(machine.SetFloat(place, 1.5f));
    EXPECT_EQ(machine.GetFloat(place), 1.5f);
    EXPECT_TRUE(machine.SetInteger(place, 9));
    EXPECT_EQ(machine.GetInteger(place), 9);
    EXPECT_TRUE(machine.SetVector(place, {1.0f, 2.0f, 3.0f}));
    EXPECT_THAT(machine.GetVector(place), ElementsAre(1.0f, 2.0f, 3.0f));

    EXPECT_FALSE(machine.SetFloat(31, 1.0f));
    EXPECT_FALSE(machine.SetInteger(-1, 1));
    // a vector whose last cell would be outside
    EXPECT_FALSE(machine.SetVector(place + 1, {1.0f, 2.0f, 3.0f}));
    EXPECT_EQ(machine.GetFloat(31), 0.0f);
    EXPECT_EQ(machine.GetInteger(-1), 0);
    EXPECT_THAT(machine.GetVector(place + 1), ElementsAre(0.0f, 0.0f, 0.0f));
  }

  TEST(QcMachineTest, ReadsTheStringsOfTheFileAndThoseMadeWhileRunning)
  {
    ProgsBuilder builder;
    const std::int32_t hello = builder.String("hello");
    QcMachine machine = MakeMachine(builder);

    EXPECT_EQ(machine.GetString(0), "");
    EXPECT_EQ(machine.GetString(hello), "hello");

    const std::int32_t first = machine.AddString("made while running");
    const std::int32_t second = machine.AddString("another");
    EXPECT_LT(first, 0);
    EXPECT_NE(first, second);
    EXPECT_TRUE(machine.HasString(first));
    EXPECT_EQ(machine.GetString(first), "made while running");
    EXPECT_EQ(machine.GetString(second), "another");
    EXPECT_EQ(machine.GetString(hello), "hello");

    // the same text is kept once
    EXPECT_EQ(machine.AddString("another"), second);

    EXPECT_FALSE(machine.HasString(second - 1));
    EXPECT_EQ(machine.GetString(second - 1), "");
    EXPECT_FALSE(machine.HasString(INT32_MIN));
    EXPECT_FALSE(machine.HasString(100000));
  }

  TEST(QcMachineTest, KeepsAStringMadeWhileRunningWhereItIsWhenMoreAreMade)
  {
    QcMachine machine = MakeMachine(ProgsBuilder());

    const std::string_view first = machine.GetString(machine.AddString("first"));
    for (int i = 0; i < 1000; i++) { machine.AddString("string " + std::to_string(i)); }
    EXPECT_EQ(first, "first");
    EXPECT_EQ(machine.GetString(-1).data(), first.data());
  }

  TEST(QcMachineTest, StartsWithTheWorldAndMakesEntitiesWithEveryFieldZero)
  {
    ProgsBuilder builder;
    builder.Field("origin", ProgsType::Vector);
    const std::uint16_t health = builder.Field("health", ProgsType::Float);
    QcMachine machine = MakeMachine(builder);

    EXPECT_EQ(machine.GetEntityCount(), 1);
    EXPECT_FALSE(machine.IsEntityFree(0));
    EXPECT_EQ(machine.GetEntity(0).size(), 4u);

    EXPECT_EQ(machine.CreateEntity(), 1);
    EXPECT_EQ(machine.CreateEntity(), 2);
    EXPECT_EQ(machine.GetEntityCount(), 3);

    machine.GetEntity(1)[health] = QcCell::OfFloat(100.0f);
    EXPECT_EQ(machine.GetEntity(1)[health].AsFloat(), 100.0f);
    EXPECT_EQ(machine.GetEntity(2)[health].AsFloat(), 0.0f);

    EXPECT_TRUE(machine.GetEntity(3).empty());
    EXPECT_TRUE(machine.GetEntity(-1).empty());
  }

  TEST(QcMachineTest, FreesAnEntityAndGivesItsNumberOutAgainWithFieldsOfZero)
  {
    ProgsBuilder builder;
    const std::uint16_t health = builder.Field("health", ProgsType::Float);
    QcMachine machine = MakeMachine(builder);
    ASSERT_EQ(machine.CreateEntity(), 1);
    ASSERT_EQ(machine.CreateEntity(), 2);
    machine.GetEntity(1)[health] = QcCell::OfFloat(100.0f);

    EXPECT_TRUE(machine.FreeEntity(1));
    EXPECT_TRUE(machine.IsEntityFree(1));
    EXPECT_FALSE(machine.IsEntityFree(2));
    EXPECT_EQ(machine.GetEntityCount(), 3);

    EXPECT_EQ(machine.CreateEntity(), 1);
    EXPECT_FALSE(machine.IsEntityFree(1));
    EXPECT_EQ(machine.GetEntity(1)[health].AsFloat(), 0.0f);
    EXPECT_EQ(machine.CreateEntity(), 3);
  }

  TEST(QcMachineTest, NeverFreesTheWorldOrAnEntityThatThereIsNot)
  {
    QcMachine machine = MakeMachine(ProgsBuilder());

    EXPECT_FALSE(machine.FreeEntity(0));
    EXPECT_FALSE(machine.FreeEntity(1));
    EXPECT_FALSE(machine.FreeEntity(-1));
    EXPECT_FALSE(machine.IsEntityFree(0));
    EXPECT_TRUE(machine.IsEntityFree(1));
  }

  TEST(QcMachineTest, MakesNoMoreEntitiesThanItsLimit)
  {
    QcLimits limits;
    limits.entities = 3;
    QcMachine machine = MakeMachine(ProgsBuilder(), limits);

    EXPECT_EQ(machine.CreateEntity(), 1);
    EXPECT_EQ(machine.CreateEntity(), 2);
    EXPECT_EQ(machine.CreateEntity(), std::nullopt);
    EXPECT_EQ(machine.GetEntityCount(), 3);
  }

  TEST(QcMachineTest, AddsSubtractsMultipliesAndDividesFloats)
  {
    ProgsBuilder builder;
    const std::uint16_t six = builder.Float(6.0f);
    const std::uint16_t four = builder.Float(4.0f);
    const std::uint16_t sum = builder.Float();
    const std::uint16_t difference = builder.Float();
    const std::uint16_t product = builder.Float();
    const std::uint16_t quotient = builder.Float();
    builder.Emit(ProgsOpcode::AddF, six, four, sum);
    builder.Emit(ProgsOpcode::SubF, six, four, difference);
    builder.Emit(ProgsOpcode::MulF, six, four, product);
    builder.Emit(ProgsOpcode::DivF, six, four, quotient);
    const QcMachine machine = RunMain(builder);

    EXPECT_EQ(machine.GetFloat(sum), 10.0f);
    EXPECT_EQ(machine.GetFloat(difference), 2.0f);
    EXPECT_EQ(machine.GetFloat(product), 24.0f);
    EXPECT_EQ(machine.GetFloat(quotient), 1.5f);
  }

  TEST(QcMachineTest, DividesByZeroIntoAnInfinityAndGoesOn)
  {
    ProgsBuilder builder;
    const std::uint16_t one = builder.Float(1.0f);
    const std::uint16_t zero = builder.Float(0.0f);
    const std::uint16_t infinite = builder.Float();
    const std::uint16_t undefined = builder.Float();
    const std::uint16_t after = builder.Float();
    builder.Emit(ProgsOpcode::DivF, one, zero, infinite);
    builder.Emit(ProgsOpcode::DivF, zero, zero, undefined);
    builder.Emit(ProgsOpcode::AddF, one, one, after);
    const QcMachine machine = RunMain(builder);

    EXPECT_TRUE(std::isinf(machine.GetFloat(infinite)));
    EXPECT_GT(machine.GetFloat(infinite), 0.0f);
    EXPECT_TRUE(std::isnan(machine.GetFloat(undefined)));
    EXPECT_EQ(machine.GetFloat(after), 2.0f);
  }

  TEST(QcMachineTest, AddsSubtractsDotsAndScalesVectors)
  {
    ProgsBuilder builder;
    const std::uint16_t left = builder.Vector(1.0f, 2.0f, 3.0f);
    const std::uint16_t right = builder.Vector(4.0f, -5.0f, 6.0f);
    const std::uint16_t two = builder.Float(2.0f);
    const std::uint16_t sum = builder.Vector();
    const std::uint16_t difference = builder.Vector();
    const std::uint16_t dot = builder.Float();
    const std::uint16_t scaled_before = builder.Vector();
    const std::uint16_t scaled_after = builder.Vector();
    builder.Emit(ProgsOpcode::AddV, left, right, sum);
    builder.Emit(ProgsOpcode::SubV, left, right, difference);
    builder.Emit(ProgsOpcode::MulV, left, right, dot);
    builder.Emit(ProgsOpcode::MulFV, two, left, scaled_before);
    builder.Emit(ProgsOpcode::MulVF, right, two, scaled_after);
    const QcMachine machine = RunMain(builder);

    EXPECT_THAT(machine.GetVector(sum), ElementsAre(5.0f, -3.0f, 9.0f));
    EXPECT_THAT(machine.GetVector(difference), ElementsAre(-3.0f, 7.0f, -3.0f));
    EXPECT_EQ(machine.GetFloat(dot), 12.0f);
    EXPECT_THAT(machine.GetVector(scaled_before), ElementsAre(2.0f, 4.0f, 6.0f));
    EXPECT_THAT(machine.GetVector(scaled_after), ElementsAre(8.0f, -10.0f, 12.0f));
  }

  TEST(QcMachineTest, WritesAVectorOverOneItReadsFrom)
  {
    ProgsBuilder builder;
    // four floats in a row, read as two vectors that share two cells
    const std::uint16_t cells = builder.Float(1.0f);
    builder.Float(2.0f);
    builder.Float(3.0f);
    builder.Float(4.0f);
    const std::uint16_t two = builder.Float(2.0f);
    builder.Emit(ProgsOpcode::MulVF, cells, two, cells + 1);
    builder.Emit(ProgsOpcode::AddV, cells + 1, cells + 1, cells);
    const QcMachine machine = RunMain(builder);

    // scaled to (2, 4, 6) one cell on, then doubled into the first three
    EXPECT_THAT(machine.GetVector(cells), ElementsAre(4.0f, 8.0f, 12.0f));
    EXPECT_EQ(machine.GetFloat(cells + 3), 6.0f);
  }

  TEST(QcMachineTest, WorksOnTheBitsOfFloatsAsWholeNumbers)
  {
    ProgsBuilder builder;
    const std::uint16_t twelve = builder.Float(12.75f);
    const std::uint16_t ten = builder.Float(10.0f);
    const std::uint16_t not_a_number = builder.Float(std::nanf(""));
    const std::uint16_t huge = builder.Float(1e30f);
    const std::uint16_t both = builder.Float();
    const std::uint16_t either = builder.Float();
    const std::uint16_t of_nothing = builder.Float(9.0f);
    const std::uint16_t of_huge = builder.Float();
    builder.Emit(ProgsOpcode::BitAnd, twelve, ten, both);
    builder.Emit(ProgsOpcode::BitOr, twelve, ten, either);
    builder.Emit(ProgsOpcode::BitOr, not_a_number, ten, of_nothing);
    builder.Emit(ProgsOpcode::BitAnd, huge, ten, of_huge);
    const QcMachine machine = RunMain(builder);

    EXPECT_EQ(machine.GetFloat(both), 8.0f);
    EXPECT_EQ(machine.GetFloat(either), 14.0f);
    // a float no whole number can hold does not break anything
    EXPECT_EQ(machine.GetFloat(of_nothing), 10.0f);
    EXPECT_EQ(machine.GetFloat(of_huge), 10.0f);
  }

  TEST(QcMachineTest, JoinsTwoTruthsWithAndAndOr)
  {
    ProgsBuilder builder;
    const std::uint16_t yes = builder.Float(3.0f);
    const std::uint16_t no = builder.Float(0.0f);
    const std::uint16_t results = builder.Vector();
    const std::uint16_t more = builder.Float();
    builder.Emit(ProgsOpcode::And, yes, yes, results);
    builder.Emit(ProgsOpcode::And, yes, no, results + 1);
    builder.Emit(ProgsOpcode::Or, yes, no, results + 2);
    builder.Emit(ProgsOpcode::Or, no, no, more);
    const QcMachine machine = RunMain(builder);

    EXPECT_THAT(machine.GetVector(results), ElementsAre(1.0f, 0.0f, 1.0f));
    EXPECT_EQ(machine.GetFloat(more), 0.0f);
  }

  TEST(QcMachineTest, ComparesFloats)
  {
    ProgsBuilder builder;
    const std::uint16_t two = builder.Float(2.0f);
    const std::uint16_t three = builder.Float(3.0f);
    const std::uint16_t less = builder.Vector();
    const std::uint16_t more = builder.Vector();
    const std::uint16_t same = builder.Vector();
    builder.Emit(ProgsOpcode::Lt, two, three, less);
    builder.Emit(ProgsOpcode::Lt, three, two, less + 1);
    builder.Emit(ProgsOpcode::Le, two, two, less + 2);
    builder.Emit(ProgsOpcode::Gt, two, three, more);
    builder.Emit(ProgsOpcode::Gt, three, two, more + 1);
    builder.Emit(ProgsOpcode::Ge, two, three, more + 2);
    builder.Emit(ProgsOpcode::EqF, two, two, same);
    builder.Emit(ProgsOpcode::EqF, two, three, same + 1);
    builder.Emit(ProgsOpcode::NeF, two, three, same + 2);
    const QcMachine machine = RunMain(builder);

    EXPECT_THAT(machine.GetVector(less), ElementsAre(1.0f, 0.0f, 1.0f));
    EXPECT_THAT(machine.GetVector(more), ElementsAre(0.0f, 1.0f, 0.0f));
    EXPECT_THAT(machine.GetVector(same), ElementsAre(1.0f, 0.0f, 1.0f));
  }

  TEST(QcMachineTest, ComparesVectorsEntitiesAndFunctions)
  {
    ProgsBuilder builder;
    const std::uint16_t here = builder.Vector(1.0f, 2.0f, 3.0f);
    const std::uint16_t also_here = builder.Vector(1.0f, 2.0f, 3.0f);
    const std::uint16_t there = builder.Vector(1.0f, 2.0f, 4.0f);
    const std::uint16_t first = builder.Integer(1);
    const std::uint16_t also_first = builder.Integer(1);
    const std::uint16_t second = builder.Integer(2);
    const std::uint16_t vectors = builder.Vector();
    const std::uint16_t entities = builder.Vector();
    const std::uint16_t functions = builder.Vector();
    builder.Emit(ProgsOpcode::EqV, here, also_here, vectors);
    builder.Emit(ProgsOpcode::EqV, here, there, vectors + 1);
    builder.Emit(ProgsOpcode::NeV, here, there, vectors + 2);
    builder.Emit(ProgsOpcode::EqE, first, also_first, entities);
    builder.Emit(ProgsOpcode::EqE, first, second, entities + 1);
    builder.Emit(ProgsOpcode::NeE, first, second, entities + 2);
    builder.Emit(ProgsOpcode::EqFnc, first, also_first, functions);
    builder.Emit(ProgsOpcode::NeFnc, first, also_first, functions + 1);
    builder.Emit(ProgsOpcode::NeFnc, first, second, functions + 2);
    const QcMachine machine = RunMain(builder);

    EXPECT_THAT(machine.GetVector(vectors), ElementsAre(1.0f, 0.0f, 1.0f));
    EXPECT_THAT(machine.GetVector(entities), ElementsAre(1.0f, 0.0f, 1.0f));
    EXPECT_THAT(machine.GetVector(functions), ElementsAre(1.0f, 0.0f, 1.0f));
  }

  TEST(QcMachineTest, ComparesStringsByTheirTextWhereverTheyAreKept)
  {
    ProgsBuilder builder;
    const std::uint16_t of_file = builder.StringGlobal("rocket");
    const std::uint16_t made = builder.Integer();
    const std::uint16_t other = builder.StringGlobal("nail");
    const std::uint16_t results = builder.Vector();
    const std::uint16_t more = builder.Float();
    builder.Emit(ProgsOpcode::EqS, of_file, made, results);
    builder.Emit(ProgsOpcode::EqS, of_file, other, results + 1);
    builder.Emit(ProgsOpcode::NeS, of_file, other, results + 2);
    builder.Emit(ProgsOpcode::NeS, of_file, made, more);
    EndMain(builder);
    QcMachine machine = MakeMachine(builder);

    // the same text at another offset, as a builtin would return it
    ASSERT_TRUE(machine.SetInteger(made, machine.AddString("rocket")));
    ASSERT_NE(machine.GetInteger(made), machine.GetInteger(of_file));
    ASSERT_TRUE(machine.Call("main")) << machine.GetError().message;

    EXPECT_THAT(machine.GetVector(results), ElementsAre(1.0f, 0.0f, 1.0f));
    EXPECT_EQ(machine.GetFloat(more), 0.0f);
  }

  TEST(QcMachineTest, SaysWhatIsNothingWithTheNotFamily)
  {
    ProgsBuilder builder;
    const std::uint16_t zero = builder.Float(0.0f);
    const std::uint16_t half = builder.Float(0.5f);
    const std::uint16_t origin = builder.Vector();
    const std::uint16_t up = builder.Vector(0.0f, 0.0f, 1.0f);
    const std::uint16_t no_string = builder.Integer(0);
    const std::uint16_t text = builder.StringGlobal("text");
    const std::uint16_t world = builder.Integer(0);
    const std::uint16_t something = builder.Integer(1);
    const std::uint16_t floats = builder.Vector();
    const std::uint16_t vectors = builder.Vector();
    const std::uint16_t strings = builder.Vector();
    const std::uint16_t entities = builder.Vector();
    const std::uint16_t functions = builder.Vector();
    builder.Emit(ProgsOpcode::NotF, zero, 0, floats);
    builder.Emit(ProgsOpcode::NotF, half, 0, floats + 1);
    builder.Emit(ProgsOpcode::NotV, origin, 0, vectors);
    builder.Emit(ProgsOpcode::NotV, up, 0, vectors + 1);
    builder.Emit(ProgsOpcode::NotS, no_string, 0, strings);
    builder.Emit(ProgsOpcode::NotS, text, 0, strings + 1);
    builder.Emit(ProgsOpcode::NotEnt, world, 0, entities);
    builder.Emit(ProgsOpcode::NotEnt, something, 0, entities + 1);
    builder.Emit(ProgsOpcode::NotFnc, world, 0, functions);
    builder.Emit(ProgsOpcode::NotFnc, something, 0, functions + 1);
    const QcMachine machine = RunMain(builder);

    EXPECT_THAT(machine.GetVector(floats), ElementsAre(1.0f, 0.0f, 0.0f));
    EXPECT_THAT(machine.GetVector(vectors), ElementsAre(1.0f, 0.0f, 0.0f));
    EXPECT_THAT(machine.GetVector(strings), ElementsAre(1.0f, 0.0f, 0.0f));
    EXPECT_THAT(machine.GetVector(entities), ElementsAre(1.0f, 0.0f, 0.0f));
    EXPECT_THAT(machine.GetVector(functions), ElementsAre(1.0f, 0.0f, 0.0f));
  }

  TEST(QcMachineTest, StoresOneGlobalInAnother)
  {
    ProgsBuilder builder;
    const std::uint16_t number = builder.Float(1.5f);
    const std::uint16_t place = builder.Vector(1.0f, 2.0f, 3.0f);
    const std::uint16_t text = builder.StringGlobal("text");
    const std::uint16_t entity = builder.Integer(3);
    const std::uint16_t field = builder.Integer(7);
    const std::uint16_t function = builder.Integer(1);
    const std::uint16_t to_number = builder.Float();
    const std::uint16_t to_place = builder.Vector();
    const std::uint16_t to_text = builder.Integer();
    const std::uint16_t to_entity = builder.Integer();
    const std::uint16_t to_field = builder.Integer();
    const std::uint16_t to_function = builder.Integer();
    builder.Emit(ProgsOpcode::StoreF, number, to_number);
    builder.Emit(ProgsOpcode::StoreV, place, to_place);
    builder.Emit(ProgsOpcode::StoreS, text, to_text);
    builder.Emit(ProgsOpcode::StoreEnt, entity, to_entity);
    builder.Emit(ProgsOpcode::StoreFld, field, to_field);
    builder.Emit(ProgsOpcode::StoreFnc, function, to_function);
    const QcMachine machine = RunMain(builder);

    EXPECT_EQ(machine.GetFloat(to_number), 1.5f);
    EXPECT_THAT(machine.GetVector(to_place), ElementsAre(1.0f, 2.0f, 3.0f));
    EXPECT_EQ(machine.GetString(machine.GetInteger(to_text)), "text");
    EXPECT_EQ(machine.GetInteger(to_entity), 3);
    EXPECT_EQ(machine.GetInteger(to_field), 7);
    EXPECT_EQ(machine.GetInteger(to_function), 1);
  }

  TEST(QcMachineTest, SumsTheNumbersFromOneToTenInALoop)
  {
    ProgsBuilder builder;
    const std::uint16_t i = builder.Float(1.0f);
    const std::uint16_t sum = builder.Float(0.0f);
    const std::uint16_t one = builder.Float(1.0f);
    const std::uint16_t ten = builder.Float(10.0f);
    const std::uint16_t going = builder.Float();

    const std::int32_t top = builder.Emit(ProgsOpcode::Le, i, ten, going);
    // out of the loop, past the three statements that follow
    builder.Emit(ProgsOpcode::IfNot, going, 4);
    builder.Emit(ProgsOpcode::AddF, sum, i, sum);
    builder.Emit(ProgsOpcode::AddF, i, one, i);
    builder.Emit(ProgsOpcode::Goto, builder.JumpTo(top));
    builder.Emit(ProgsOpcode::Return, sum);
    const QcMachine machine = RunMain(builder);

    EXPECT_EQ(machine.GetFloat(sum), 55.0f);
    EXPECT_EQ(machine.GetFloat(i), 11.0f);
    EXPECT_EQ(machine.GetFloat(QcMachine::return_offset), 55.0f);
  }

  TEST(QcMachineTest, JumpsWithIfWhenTrueAndWithIfNotWhenFalse)
  {
    ProgsBuilder builder;
    const std::uint16_t yes = builder.Float(1.0f);
    const std::uint16_t no = builder.Float(0.0f);
    const std::uint16_t five = builder.Float(5.0f);
    const std::uint16_t skipped = builder.Vector();
    const std::uint16_t reached = builder.Vector();
    builder.Emit(ProgsOpcode::If, yes, 2);
    builder.Emit(ProgsOpcode::StoreF, five, skipped);
    builder.Emit(ProgsOpcode::IfNot, no, 2);
    builder.Emit(ProgsOpcode::StoreF, five, skipped + 1);
    builder.Emit(ProgsOpcode::If, no, 2);
    builder.Emit(ProgsOpcode::StoreF, five, reached);
    builder.Emit(ProgsOpcode::IfNot, yes, 2);
    builder.Emit(ProgsOpcode::StoreF, five, reached + 1);
    const QcMachine machine = RunMain(builder);

    EXPECT_THAT(machine.GetVector(skipped), ElementsAre(0.0f, 0.0f, 0.0f));
    EXPECT_THAT(machine.GetVector(reached), ElementsAre(5.0f, 5.0f, 0.0f));
  }

  TEST(QcMachineTest, TakesMinusZeroForTrueInIfAndForNothingInNotAsTheOriginalDoes)
  {
    ProgsBuilder builder;
    const std::uint16_t minus_zero = builder.Float(-0.0f);
    const std::uint16_t five = builder.Float(5.0f);
    const std::uint16_t skipped = builder.Float();
    const std::uint16_t nothing = builder.Float();
    builder.Emit(ProgsOpcode::If, minus_zero, 2);
    builder.Emit(ProgsOpcode::StoreF, five, skipped);
    builder.Emit(ProgsOpcode::NotF, minus_zero, 0, nothing);
    const QcMachine machine = RunMain(builder);

    EXPECT_EQ(machine.GetFloat(skipped), 0.0f);
    EXPECT_EQ(machine.GetFloat(nothing), 1.0f);
  }

  TEST(QcMachineTest, ReturnsAValueOfThreeCellsAndEndsThere)
  {
    ProgsBuilder builder;
    const std::uint16_t place = builder.Vector(1.0f, 2.0f, 3.0f);
    const std::uint16_t five = builder.Float(5.0f);
    const std::uint16_t after = builder.Float();
    builder.Emit(ProgsOpcode::Return, place);
    builder.Emit(ProgsOpcode::StoreF, five, after);
    const QcMachine machine = RunMain(builder);

    EXPECT_THAT(machine.GetVector(QcMachine::return_offset), ElementsAre(1.0f, 2.0f, 3.0f));
    EXPECT_EQ(machine.GetFloat(after), 0.0f);
  }

  TEST(QcMachineTest, RunsAFunctionByItsNumberOrItsName)
  {
    ProgsBuilder builder;
    const std::uint16_t one = builder.Float(1.0f);
    const std::uint16_t count = builder.Float();
    builder.Emit(ProgsOpcode::AddF, count, one, count);
    EndMain(builder);
    QcMachine machine = MakeMachine(builder);

    EXPECT_TRUE(machine.Call(1));
    EXPECT_TRUE(machine.Call("main"));
    EXPECT_EQ(machine.GetFloat(count), 2.0f);
  }

  TEST(QcMachineTest, ReadsTheFieldsOfAnEntityWithLoad)
  {
    ProgsBuilder builder;
    const std::uint16_t origin = builder.Field("origin", ProgsType::Vector);
    const std::uint16_t health = builder.Field("health", ProgsType::Float);
    const std::uint16_t name = builder.Field("classname", ProgsType::String);
    const std::uint16_t owner = builder.Field("owner", ProgsType::Entity);
    const std::uint16_t which = builder.Field("which", ProgsType::Field);
    const std::uint16_t think = builder.Field("think", ProgsType::Function);
    const std::int32_t monster = builder.String("monster");

    const std::uint16_t entity = builder.Integer(1);
    const std::uint16_t origin_field = builder.Integer(origin);
    const std::uint16_t health_field = builder.Integer(health);
    const std::uint16_t name_field = builder.Integer(name);
    const std::uint16_t owner_field = builder.Integer(owner);
    const std::uint16_t which_field = builder.Integer(which);
    const std::uint16_t think_field = builder.Integer(think);
    const std::uint16_t got_origin = builder.Vector();
    const std::uint16_t got_health = builder.Float();
    const std::uint16_t got_name = builder.Integer();
    const std::uint16_t got_owner = builder.Integer();
    const std::uint16_t got_which = builder.Integer();
    const std::uint16_t got_think = builder.Integer();
    builder.Emit(ProgsOpcode::LoadV, entity, origin_field, got_origin);
    builder.Emit(ProgsOpcode::LoadF, entity, health_field, got_health);
    builder.Emit(ProgsOpcode::LoadS, entity, name_field, got_name);
    builder.Emit(ProgsOpcode::LoadEnt, entity, owner_field, got_owner);
    builder.Emit(ProgsOpcode::LoadFld, entity, which_field, got_which);
    builder.Emit(ProgsOpcode::LoadFnc, entity, think_field, got_think);
    EndMain(builder);
    QcMachine machine = MakeMachine(builder);

    ASSERT_EQ(machine.CreateEntity(), 1);
    const std::span<QcCell> fields = machine.GetEntity(1);
    fields[origin] = QcCell::OfFloat(16.0f);
    fields[origin + 1] = QcCell::OfFloat(32.0f);
    fields[origin + 2] = QcCell::OfFloat(-8.0f);
    fields[health] = QcCell::OfFloat(100.0f);
    fields[name] = QcCell::OfInteger(monster);
    fields[owner] = QcCell::OfInteger(0);
    fields[which] = QcCell::OfInteger(health);
    fields[think] = QcCell::OfInteger(1);
    ASSERT_TRUE(machine.Call("main")) << machine.GetError().message;

    EXPECT_THAT(machine.GetVector(got_origin), ElementsAre(16.0f, 32.0f, -8.0f));
    EXPECT_EQ(machine.GetFloat(got_health), 100.0f);
    EXPECT_EQ(machine.GetString(machine.GetInteger(got_name)), "monster");
    EXPECT_EQ(machine.GetInteger(got_owner), 0);
    EXPECT_EQ(machine.GetInteger(got_which), health);
    EXPECT_EQ(machine.GetInteger(got_think), 1);
  }

  TEST(QcMachineTest, WritesTheFieldsOfAnEntityThroughAPointerOfAddress)
  {
    ProgsBuilder builder;
    const std::uint16_t origin = builder.Field("origin", ProgsType::Vector);
    const std::uint16_t health = builder.Field("health", ProgsType::Float);
    const std::uint16_t name = builder.Field("classname", ProgsType::String);
    const std::uint16_t owner = builder.Field("owner", ProgsType::Entity);
    const std::uint16_t which = builder.Field("which", ProgsType::Field);
    const std::uint16_t think = builder.Field("think", ProgsType::Function);

    const std::uint16_t entity = builder.Integer(2);
    const std::uint16_t fields_at = builder.Integer(origin);
    builder.Integer(health);
    builder.Integer(name);
    builder.Integer(owner);
    builder.Integer(which);
    builder.Integer(think);
    const std::uint16_t place = builder.Vector(1.0f, 2.0f, 3.0f);
    const std::uint16_t hundred = builder.Float(100.0f);
    const std::uint16_t text = builder.StringGlobal("monster");
    const std::uint16_t one = builder.Integer(1);
    const std::uint16_t pointer = builder.Integer();
    const ProgsOpcode stores[] = {
      ProgsOpcode::StorepV, ProgsOpcode::StorepF, ProgsOpcode::StorepS,
      ProgsOpcode::StorepEnt, ProgsOpcode::StorepFld, ProgsOpcode::StorepFnc,
    };
    const std::uint16_t values[] = {place, hundred, text, one, one, one};
    for (std::uint16_t i = 0; i < 6; i++)
    {
      builder.Emit(ProgsOpcode::Address, entity, fields_at + i, pointer);
      builder.Emit(stores[i], values[i], pointer);
    }
    EndMain(builder);
    QcMachine machine = MakeMachine(builder);

    ASSERT_EQ(machine.CreateEntity(), 1);
    ASSERT_EQ(machine.CreateEntity(), 2);
    ASSERT_TRUE(machine.Call("main")) << machine.GetError().message;

    const std::span<QcCell> fields = machine.GetEntity(2);
    EXPECT_EQ(fields[origin].AsFloat(), 1.0f);
    EXPECT_EQ(fields[origin + 1].AsFloat(), 2.0f);
    EXPECT_EQ(fields[origin + 2].AsFloat(), 3.0f);
    EXPECT_EQ(fields[health].AsFloat(), 100.0f);
    EXPECT_EQ(machine.GetString(fields[name].AsInteger()), "monster");
    EXPECT_EQ(fields[owner].AsInteger(), 1);
    EXPECT_EQ(fields[which].AsInteger(), 1);
    EXPECT_EQ(fields[think].AsInteger(), 1);

    // nothing of the entity before it was touched
    for (const QcCell cell : machine.GetEntity(1)) { EXPECT_EQ(cell.bits, 0u); }
  }

  TEST(QcMachineTest, ReadsAndWritesAFreeEntityAsTheOriginalDoes)
  {
    ProgsBuilder builder;
    const std::uint16_t health = builder.Field("health", ProgsType::Float);
    const std::uint16_t entity = builder.Integer(1);
    const std::uint16_t health_field = builder.Integer(health);
    const std::uint16_t hundred = builder.Float(100.0f);
    const std::uint16_t pointer = builder.Integer();
    const std::uint16_t got = builder.Float();
    builder.Emit(ProgsOpcode::Address, entity, health_field, pointer);
    builder.Emit(ProgsOpcode::StorepF, hundred, pointer);
    builder.Emit(ProgsOpcode::LoadF, entity, health_field, got);
    EndMain(builder);
    QcMachine machine = MakeMachine(builder);
    ASSERT_EQ(machine.CreateEntity(), 1);
    ASSERT_TRUE(machine.FreeEntity(1));

    ASSERT_TRUE(machine.Call("main")) << machine.GetError().message;
    EXPECT_EQ(machine.GetFloat(got), 100.0f);
  }

  TEST(QcMachineTest, SetsTheFrameAndTheNextThinkOfSelfWithState)
  {
    ProgsBuilder builder;
    builder.Field("health", ProgsType::Float);
    const std::uint16_t next_think = builder.Field("nextthink", ProgsType::Float);
    const std::uint16_t frame = builder.Field("frame", ProgsType::Float);
    const std::uint16_t think = builder.Field("think", ProgsType::Function);
    const std::uint16_t self = builder.Integer(2);
    builder.Name(self, "self", ProgsType::Entity);
    const std::uint16_t time = builder.Float(10.0f);
    builder.Name(time, "time", ProgsType::Float);
    const std::uint16_t seven = builder.Float(7.0f);
    const std::uint16_t function = builder.Integer(1);
    builder.Emit(ProgsOpcode::State, seven, function);
    EndMain(builder);
    QcMachine machine = MakeMachine(builder);
    ASSERT_EQ(machine.CreateEntity(), 1);
    ASSERT_EQ(machine.CreateEntity(), 2);

    ASSERT_TRUE(machine.Call("main")) << machine.GetError().message;
    const std::span<QcCell> fields = machine.GetEntity(2);
    EXPECT_FLOAT_EQ(fields[next_think].AsFloat(), 10.1f);
    EXPECT_EQ(fields[frame].AsFloat(), 7.0f);
    EXPECT_EQ(fields[think].AsInteger(), 1);
    for (const QcCell cell : machine.GetEntity(1)) { EXPECT_EQ(cell.bits, 0u); }
  }

  /// The global a parameter of a call is put in.
  std::uint16_t Parameter(const int index)
  {
    return static_cast<std::uint16_t>(QcMachine::first_parameter_offset + index * QcMachine::parameter_size);
  }

  constexpr std::uint16_t returned = QcMachine::return_offset;

  TEST(QcMachineTest, CallsAFunctionWithParametersAndTakesWhatItReturns)
  {
    ProgsBuilder builder;
    // float(float a, float b) difference = { return a - b; }
    const std::uint16_t a = builder.Float();
    const std::uint16_t b = builder.Float();
    const std::uint16_t result = builder.Float();
    const std::int32_t start = builder.Emit(ProgsOpcode::SubF, a, b, result);
    builder.Emit(ProgsOpcode::Return, result);
    const std::int32_t difference = builder.Function("difference", start, a, 3, {1, 1});

    const std::uint16_t nine = builder.Float(9.0f);
    const std::uint16_t four = builder.Float(4.0f);
    const std::uint16_t function = builder.Integer(difference);
    const std::uint16_t got = builder.Float();
    const std::int32_t main = builder.Emit(ProgsOpcode::StoreF, nine, Parameter(0));
    builder.Emit(ProgsOpcode::StoreF, four, Parameter(1));
    builder.Emit(ProgsOpcode::Call2, function);
    builder.Emit(ProgsOpcode::StoreF, returned, got);
    builder.Emit(ProgsOpcode::Done);
    builder.Function("main", main);
    QcMachine machine = MakeMachine(builder);

    ASSERT_TRUE(machine.Call("main")) << machine.GetError().message;
    EXPECT_EQ(machine.GetFloat(got), 5.0f);
  }

  TEST(QcMachineTest, IsCalledByTheHostWithParametersOfEveryKind)
  {
    ProgsBuilder builder;
    // vector(vector v, float s) scale = { return v * s; }
    const std::uint16_t v = builder.Vector();
    const std::uint16_t s = builder.Float();
    const std::uint16_t result = builder.Vector();
    const std::int32_t scale = builder.Emit(ProgsOpcode::MulVF, v, s, result);
    builder.Emit(ProgsOpcode::Return, result);
    builder.Function("scale", scale, v, 7, {3, 1});

    // string(float unused, string text) echo = { return text; }
    const std::uint16_t unused = builder.Float();
    const std::uint16_t text = builder.Integer();
    const std::int32_t echo = builder.Emit(ProgsOpcode::Return, text);
    builder.Function("echo", echo, unused, 2, {1, 1});
    QcMachine machine = MakeMachine(builder);

    ASSERT_TRUE(machine.SetParameterVector(0, {1.0f, 2.0f, 3.0f}));
    ASSERT_TRUE(machine.SetParameterFloat(1, 3.0f));
    ASSERT_TRUE(machine.Call("scale")) << machine.GetError().message;
    EXPECT_THAT(machine.GetReturnVector(), ElementsAre(3.0f, 6.0f, 9.0f));
    EXPECT_EQ(machine.GetReturnFloat(), 3.0f);

    ASSERT_TRUE(machine.SetParameterString(1, "from the host"));
    ASSERT_TRUE(machine.Call("echo")) << machine.GetError().message;
    EXPECT_EQ(machine.GetReturnString(), "from the host");
    EXPECT_LT(machine.GetReturnInteger(), 0);

    EXPECT_FALSE(machine.SetParameterFloat(8, 1.0f));
    EXPECT_FALSE(machine.SetParameterInteger(-1, 1));
    EXPECT_FALSE(machine.SetParameterString(8, "nowhere"));
    EXPECT_EQ(machine.GetParameterFloat(8), 0.0f);
  }

  /// Adds `float(float n) factorial`, which calls itself, and gives its
  /// number. `n` is the global of its parameter.
  std::int32_t AddFactorial(ProgsBuilder &builder, std::uint16_t &n)
  {
    // if (n <= 1) return 1; return n * factorial(n - 1);
    const std::uint16_t one = builder.Float(1.0f);
    const std::uint16_t itself = builder.Integer(builder.NextFunction());
    n = builder.Float();
    const std::uint16_t small = builder.Float();
    const std::uint16_t result = builder.Float();
    const std::int32_t start = builder.Emit(ProgsOpcode::Le, n, one, small);
    builder.Emit(ProgsOpcode::IfNot, small, 2);
    builder.Emit(ProgsOpcode::Return, one);
    builder.Emit(ProgsOpcode::SubF, n, one, Parameter(0));
    builder.Emit(ProgsOpcode::Call1, itself);
    // `n` is read after the call that set it to something smaller
    builder.Emit(ProgsOpcode::MulF, n, returned, result);
    builder.Emit(ProgsOpcode::Return, result);
    return builder.Function("factorial", start, n, 3, {1});
  }

  TEST(QcMachineTest, WorksOutAFactorialWithAFunctionThatCallsItself)
  {
    ProgsBuilder builder;
    std::uint16_t n = 0;
    AddFactorial(builder, n);
    QcMachine machine = MakeMachine(builder);

    machine.SetParameterFloat(0, 5.0f);
    ASSERT_TRUE(machine.Call("factorial")) << machine.GetError().message;
    EXPECT_EQ(machine.GetReturnFloat(), 120.0f);

    machine.SetParameterFloat(0, 10.0f);
    ASSERT_TRUE(machine.Call("factorial")) << machine.GetError().message;
    EXPECT_EQ(machine.GetReturnFloat(), 3628800.0f);
  }

  TEST(QcMachineTest, GivesTheLocalsOfAFunctionBackWhatTheyHeldBeforeTheCall)
  {
    ProgsBuilder builder;
    std::uint16_t n = 0;
    AddFactorial(builder, n);
    QcMachine machine = MakeMachine(builder);

    // what the three locals hold before the call
    ASSERT_TRUE(machine.SetVector(n, {70.0f, 80.0f, 90.0f}));
    machine.SetParameterFloat(0, 4.0f);
    ASSERT_TRUE(machine.Call("factorial")) << machine.GetError().message;

    EXPECT_EQ(machine.GetReturnFloat(), 24.0f);
    EXPECT_THAT(machine.GetVector(n), ElementsAre(70.0f, 80.0f, 90.0f));
  }

  TEST(QcMachineTest, CallsABuiltinAndTakesWhatItReturns)
  {
    ProgsBuilder builder;
    const std::int32_t twice = builder.Builtin("twice", 7);
    const std::uint16_t function = builder.Integer(twice);
    const std::uint16_t twenty_one = builder.Float(21.0f);
    const std::uint16_t got = builder.Float();
    builder.Emit(ProgsOpcode::StoreF, twenty_one, Parameter(0));
    builder.Emit(ProgsOpcode::Call1, function);
    builder.Emit(ProgsOpcode::StoreF, returned, got);
    EndMain(builder);
    QcMachine machine = MakeMachine(builder);

    int calls = 0;
    ASSERT_TRUE(machine.SetBuiltin(7, [&calls](QcMachine &called)
    {
      calls++;
      called.SetReturnFloat(called.GetParameterFloat(0) * 2.0f);
    }));

    ASSERT_TRUE(machine.Call("main")) << machine.GetError().message;
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(machine.GetFloat(got), 42.0f);

    // the host may call a builtin as it calls any function
    machine.SetParameterFloat(0, 4.0f);
    ASSERT_TRUE(machine.Call("twice")) << machine.GetError().message;
    EXPECT_EQ(machine.GetReturnFloat(), 8.0f);
    EXPECT_EQ(calls, 2);

    EXPECT_FALSE(machine.SetBuiltin(0, [](QcMachine &) {}));
    EXPECT_FALSE(machine.SetBuiltin(-7, [](QcMachine &) {}));
  }

  TEST(QcMachineTest, HandsABuiltinParametersOfEveryKindAndTakesAStringItMakes)
  {
    ProgsBuilder builder;
    const std::int32_t describe = builder.Builtin("describe", 3);
    const std::uint16_t function = builder.Integer(describe);
    const std::uint16_t place = builder.Vector(1.0f, 2.0f, 3.0f);
    const std::uint16_t name = builder.StringGlobal("shambler");
    const std::uint16_t entity = builder.Integer(5);
    const std::uint16_t expected = builder.StringGlobal("shambler 5 at 1 2 3");
    const std::uint16_t got = builder.Integer();
    const std::uint16_t same = builder.Float();
    builder.Emit(ProgsOpcode::StoreV, place, Parameter(0));
    builder.Emit(ProgsOpcode::StoreS, name, Parameter(1));
    builder.Emit(ProgsOpcode::StoreEnt, entity, Parameter(2));
    builder.Emit(ProgsOpcode::Call3, function);
    builder.Emit(ProgsOpcode::StoreS, returned, got);
    builder.Emit(ProgsOpcode::EqS, got, expected, same);
    EndMain(builder);
    QcMachine machine = MakeMachine(builder);

    machine.SetBuiltin(3, [](QcMachine &called)
    {
      const std::array<float, 3> at = called.GetParameterVector(0);
      called.SetReturnString(
        std::string(called.GetParameterString(1)) + " " + std::to_string(called.GetParameterInteger(2)) + " at " +
        std::to_string(static_cast<int>(at[0])) + " " + std::to_string(static_cast<int>(at[1])) + " " +
        std::to_string(static_cast<int>(at[2])));
    });

    ASSERT_TRUE(machine.Call("main")) << machine.GetError().message;
    EXPECT_EQ(machine.GetString(machine.GetInteger(got)), "shambler 5 at 1 2 3");
    EXPECT_LT(machine.GetInteger(got), 0);
    EXPECT_EQ(machine.GetFloat(same), 1.0f);
  }

  TEST(QcMachineTest, TellsABuiltinWithHowManyParametersItWasCalled)
  {
    ProgsBuilder builder;
    const std::uint16_t function = builder.Integer(builder.Builtin("count", 1));
    for (std::uint16_t i = 0; i <= 8; i++)
    {
      builder.Emit(static_cast<ProgsOpcode>(static_cast<std::uint16_t>(ProgsOpcode::Call0) + i), function);
    }
    EndMain(builder);
    QcMachine machine = MakeMachine(builder);

    std::vector<std::int32_t> counts;
    machine.SetBuiltin(1, [&counts](QcMachine &called) { counts.push_back(called.GetArgumentCount()); });

    ASSERT_TRUE(machine.Call("main")) << machine.GetError().message;
    EXPECT_THAT(counts, ElementsAre(0, 1, 2, 3, 4, 5, 6, 7, 8));
  }

  TEST(QcMachineTest, LetsABuiltinRunAFunctionOfTheProgramWhileItIsCalled)
  {
    ProgsBuilder builder;
    std::uint16_t n = 0;
    AddFactorial(builder, n);
    const std::uint16_t function = builder.Integer(builder.Builtin("callback", 2));
    const std::uint16_t got = builder.Float();
    const std::int32_t main = builder.Emit(ProgsOpcode::Call2, function);
    builder.Emit(ProgsOpcode::StoreF, returned, got);
    builder.Emit(ProgsOpcode::Done);
    builder.Function("main", main);
    QcMachine machine = MakeMachine(builder);

    std::int32_t count_after = -1;
    machine.SetBuiltin(2, [&count_after](QcMachine &called)
    {
      called.SetParameterFloat(0, 4.0f);
      EXPECT_TRUE(called.Call("factorial"));
      count_after = called.GetArgumentCount();
      called.SetReturnFloat(called.GetReturnFloat() + 1.0f);
    });

    ASSERT_TRUE(machine.Call("main")) << machine.GetError().message;
    EXPECT_EQ(machine.GetFloat(got), 25.0f);
    EXPECT_EQ(count_after, 2);
  }

  TEST(QcMachineTest, ReturnsAFloatThatIsTheLastGlobalOfTheProgram)
  {
    ProgsBuilder builder;
    builder.Vector(1.0f, 2.0f, 3.0f);
    const std::uint16_t last = builder.Float(8.0f);
    builder.Emit(ProgsOpcode::Return, last);
    QcMachine machine = RunMain(builder);

    // the two cells after it, which there are not, come back as zero
    EXPECT_THAT(machine.GetReturnVector(), ElementsAre(8.0f, 0.0f, 0.0f));
  }

  TEST(QcMachineTest, CopiesAFloatThatIsTheLastGlobalAsAVectorAsTheCompilerDoesForParameters)
  {
    ProgsBuilder builder;
    const std::uint16_t last = builder.Float(8.0f);
    builder.Emit(ProgsOpcode::StoreV, last, Parameter(0));
    QcMachine machine = RunMain(builder);

    EXPECT_THAT(machine.GetParameterVector(0), ElementsAre(8.0f, 0.0f, 0.0f));
    // the host still sees the globals the program has, and no more
    EXPECT_EQ(machine.GetGlobalCount(), 29);
    EXPECT_FALSE(machine.SetFloat(29, 1.0f));
  }

  /// Runs `main` of a builder, expects the run to be stopped with an error
  /// that says `what`, in `main` at a statement, and gives the machine.
  QcMachine ExpectStopped(
    ProgsBuilder &builder,
    const std::string &what,
    const std::int32_t statement,
    const QcLimits &limits = {})
  {
    EndMain(builder);
    QcMachine machine = MakeMachine(builder, limits);
    EXPECT_FALSE(machine.Call("main"));
    EXPECT_TRUE(machine.HasFailed());
    EXPECT_THAT(machine.GetError().message, HasSubstr(what));
    EXPECT_EQ(machine.GetError().function_name, "main");
    EXPECT_EQ(machine.GetError().function, machine.GetProgs().FindFunction("main"));
    EXPECT_EQ(machine.GetError().statement, statement);
    return machine;
  }

  TEST(QcMachineTest, StopsAtAnOpcodeItDoesNotKnow)
  {
    ProgsBuilder builder;
    const std::uint16_t one = builder.Float(1.0f);
    builder.Emit(ProgsOpcode::AddF, one, one, one);
    builder.Emit(static_cast<ProgsOpcode>(66), one, one, one);
    builder.Emit(ProgsOpcode::AddF, one, one, one);
    const QcMachine machine = ExpectStopped(builder, "Opcode 66 is not known", 2);

    // what came before the error ran, what came after did not
    EXPECT_EQ(machine.GetFloat(one), 2.0f);
  }

  TEST(QcMachineTest, StopsAtAJumpOutsideTheStatements)
  {
    ProgsBuilder forward;
    forward.Emit(ProgsOpcode::Goto, 100);
    ExpectStopped(forward, "went to statement 101, outside its 3 statements", 1);

    ProgsBuilder back;
    back.Emit(ProgsOpcode::Goto, static_cast<std::uint16_t>(-5));
    ExpectStopped(back, "went to statement -4", 1);

    ProgsBuilder conditional;
    const std::uint16_t yes = conditional.Float(1.0f);
    conditional.Emit(ProgsOpcode::If, yes, 0x7fff);
    ExpectStopped(conditional, "outside its 3 statements", 1);

    ProgsBuilder otherwise;
    const std::uint16_t no = otherwise.Float(0.0f);
    otherwise.Emit(ProgsOpcode::IfNot, no, 0x8000);
    ExpectStopped(otherwise, "outside its 3 statements", 1);
  }

  TEST(QcMachineTest, StopsWhenAFunctionRunsPastTheLastStatement)
  {
    ProgsBuilder builder;
    const std::uint16_t one = builder.Float(1.0f);
    const std::int32_t start = builder.Emit(ProgsOpcode::AddF, one, one, one);
    builder.Function("main", start);
    QcMachine machine = MakeMachine(builder);

    EXPECT_FALSE(machine.Call("main"));
    EXPECT_THAT(machine.GetError().message, HasSubstr("went to statement 2, outside its 2 statements"));
    EXPECT_EQ(machine.GetError().statement, 1);
  }

  TEST(QcMachineTest, StopsAtAGlobalOutsideTheGlobals)
  {
    ProgsBuilder reading;
    const std::uint16_t one = reading.Float(1.0f);
    reading.Emit(ProgsOpcode::AddF, 60000, one, one);
    ExpectStopped(reading, "names a global outside the 29 there are", 1);

    ProgsBuilder writing;
    const std::uint16_t two = writing.Float(2.0f);
    writing.Emit(ProgsOpcode::StoreF, two, 29);
    ExpectStopped(writing, "names a global outside the 29 there are", 1);

    ProgsBuilder vector;
    const std::uint16_t place = vector.Vector();
    vector.Emit(ProgsOpcode::AddV, place, place, 0xffff);
    ExpectStopped(vector, "names a global outside the 31 there are", 1);
  }

  TEST(QcMachineTest, StopsAtAnEntityThatThereIsNot)
  {
    for (const std::int32_t number : {1, -1, 1000000})
    {
      ProgsBuilder loading;
      const std::uint16_t health = loading.Integer(loading.Field("health", ProgsType::Float));
      const std::uint16_t entity = loading.Integer(number);
      const std::uint16_t got = loading.Float();
      loading.Emit(ProgsOpcode::LoadF, entity, health, got);
      ExpectStopped(loading, "There is no entity " + std::to_string(number), 1);

      ProgsBuilder addressing;
      const std::uint16_t field = addressing.Integer(addressing.Field("health", ProgsType::Float));
      const std::uint16_t other = addressing.Integer(number);
      const std::uint16_t pointer = addressing.Integer();
      addressing.Emit(ProgsOpcode::Address, other, field, pointer);
      ExpectStopped(addressing, "There is no entity " + std::to_string(number), 1);
    }
  }

  TEST(QcMachineTest, StopsAtAFieldOutsideAnEntity)
  {
    for (const std::int32_t offset : {4, -1, 1000000})
    {
      ProgsBuilder loading;
      loading.Field("origin", ProgsType::Vector);
      loading.Field("health", ProgsType::Float);
      const std::uint16_t field = loading.Integer(offset);
      const std::uint16_t world = loading.Integer(0);
      const std::uint16_t got = loading.Float();
      loading.Emit(ProgsOpcode::LoadF, world, field, got);
      ExpectStopped(loading, "at offset " + std::to_string(offset) + " is outside the 4 cells of an entity", 1);
    }

    // a vector that starts inside an entity and ends outside it
    ProgsBuilder vector;
    vector.Field("origin", ProgsType::Vector);
    const std::uint16_t health = vector.Integer(vector.Field("health", ProgsType::Float));
    const std::uint16_t world = vector.Integer(0);
    const std::uint16_t got = vector.Vector();
    vector.Emit(ProgsOpcode::LoadV, world, health, got);
    ExpectStopped(vector, "A field of 3 cells at offset 3 is outside the 4 cells of an entity", 1);

    ProgsBuilder none;
    const std::uint16_t zero = none.Integer(0);
    const std::uint16_t result = none.Float();
    none.Emit(ProgsOpcode::LoadF, zero, zero, result);
    ExpectStopped(none, "outside the 0 cells of an entity", 1);
  }

  TEST(QcMachineTest, StopsAtAPointerThatPointsAtNoFieldOfAnEntity)
  {
    for (const std::int32_t made_up : {-1, 4, 1000000})
    {
      ProgsBuilder builder;
      builder.Field("origin", ProgsType::Vector);
      builder.Field("health", ProgsType::Float);
      const std::uint16_t pointer = builder.Integer(made_up);
      const std::uint16_t value = builder.Float(1.0f);
      builder.Emit(ProgsOpcode::StorepF, value, pointer);
      ExpectStopped(builder, made_up < 0 ? "points at no field" : "There is no entity", 1);
    }

    // a vector written through a pointer to the last cell of the world must
    // not reach into the entity after it
    ProgsBuilder vector;
    vector.Field("origin", ProgsType::Vector);
    const std::uint16_t health = vector.Field("health", ProgsType::Float);
    const std::uint16_t pointer = vector.Integer(health);
    const std::uint16_t value = vector.Vector(1.0f, 2.0f, 3.0f);
    vector.Emit(ProgsOpcode::StorepV, value, pointer);
    EndMain(vector);
    QcMachine machine = MakeMachine(vector);
    ASSERT_EQ(machine.CreateEntity(), 1);
    EXPECT_FALSE(machine.Call("main"));
    EXPECT_THAT(machine.GetError().message, HasSubstr("A field of 3 cells at offset 3"));
    for (const QcCell cell : machine.GetEntity(0)) { EXPECT_EQ(cell.bits, 0u); }
    for (const QcCell cell : machine.GetEntity(1)) { EXPECT_EQ(cell.bits, 0u); }

    ProgsBuilder no_fields;
    const std::uint16_t zero = no_fields.Integer(0);
    no_fields.Emit(ProgsOpcode::StorepF, zero, zero);
    ExpectStopped(no_fields, "points at no field", 1);
  }

  TEST(QcMachineTest, StopsAtAStringThatThereIsNot)
  {
    ProgsBuilder comparing;
    const std::uint16_t text = comparing.StringGlobal("text");
    const std::uint16_t nowhere = comparing.Integer(100000);
    const std::uint16_t same = comparing.Float();
    comparing.Emit(ProgsOpcode::EqS, text, nowhere, same);
    ExpectStopped(comparing, "There is no string at offset 100000", 1);

    // an offset of the strings made while running, of which there are none
    ProgsBuilder testing;
    const std::uint16_t never_made = testing.Integer(-1);
    const std::uint16_t empty = testing.Float();
    testing.Emit(ProgsOpcode::NotS, never_made, 0, empty);
    ExpectStopped(testing, "There is no string at offset -1", 1);
  }

  TEST(QcMachineTest, StopsALoopThatNeverEnds)
  {
    ProgsBuilder builder;
    const std::uint16_t one = builder.Float(1.0f);
    const std::uint16_t count = builder.Float();
    builder.Emit(ProgsOpcode::AddF, count, one, count);
    builder.Emit(ProgsOpcode::Goto, static_cast<std::uint16_t>(-1));
    QcLimits limits;
    limits.statements = 1000;
    const QcMachine machine = ExpectStopped(builder, "ran more than 1000 statements", 1, limits);

    EXPECT_EQ(machine.GetFloat(count), 500.0f);
  }

  TEST(QcMachineTest, StopsAFunctionThatCallsItselfForEver)
  {
    ProgsBuilder builder;
    const std::uint16_t one = builder.Float(1.0f);
    const std::uint16_t depth = builder.Float();
    const std::uint16_t itself = builder.Integer(builder.NextFunction());
    builder.Emit(ProgsOpcode::AddF, depth, one, depth);
    builder.Emit(ProgsOpcode::Call0, itself);
    QcLimits limits;
    limits.stack_depth = 8;
    const QcMachine machine = ExpectStopped(builder, "stack is too deep", 2, limits);

    EXPECT_THAT(machine.GetError().message, HasSubstr("more than 8 calls"));
    EXPECT_EQ(machine.GetFloat(depth), 8.0f);
  }

  TEST(QcMachineTest, StopsWhenTheLocalsPutAsideAreTooMany)
  {
    ProgsBuilder builder;
    std::uint16_t n = 0;
    AddFactorial(builder, n);
    QcLimits limits;
    // three locals a call: the fifth call does not fit
    limits.saved_locals = 12;
    QcMachine machine = MakeMachine(builder, limits);

    machine.SetParameterFloat(0, 4.0f);
    EXPECT_TRUE(machine.Call("factorial"));

    machine.SetParameterFloat(0, 5.0f);
    EXPECT_FALSE(machine.Call("factorial"));
    EXPECT_THAT(machine.GetError().message, HasSubstr("more than 12 locals aside"));
    EXPECT_EQ(machine.GetError().function_name, "factorial");
  }

  TEST(QcMachineTest, StopsAtABuiltinThatIsNotRegisteredAndSaysWhich)
  {
    ProgsBuilder builder;
    const std::uint16_t function = builder.Integer(builder.Builtin("makevectors", 23));
    builder.Emit(ProgsOpcode::Call1, function);
    ExpectStopped(builder, "Builtin 23 (makevectors) is not registered", 1);

    ProgsBuilder nameless;
    const std::uint16_t unnamed = nameless.Integer(nameless.Builtin("", 99));
    nameless.Emit(ProgsOpcode::Call0, unnamed);
    ExpectStopped(nameless, "Builtin 99 is not registered", 1);

    // called by the host, the error is in no function
    ProgsBuilder alone;
    alone.Builtin("makevectors", 23);
    QcMachine machine = MakeMachine(alone);
    EXPECT_FALSE(machine.Call("makevectors"));
    EXPECT_THAT(machine.GetError().message, HasSubstr("Builtin 23 (makevectors) is not registered"));
    EXPECT_EQ(machine.GetError().function, 0);
    EXPECT_EQ(machine.GetError().statement, -1);
  }

  TEST(QcMachineTest, StopsAtACallOfAFunctionThatThereIsNot)
  {
    ProgsBuilder nothing;
    const std::uint16_t zero = nothing.Integer(0);
    nothing.Emit(ProgsOpcode::Call0, zero);
    ExpectStopped(nothing, "A call of function 0, which is no function", 1);

    for (const std::int32_t number : {2, -1, 1000000})
    {
      ProgsBuilder builder;
      const std::uint16_t function = builder.Integer(number);
      builder.Emit(ProgsOpcode::Call0, function);
      ExpectStopped(builder, "A call of function " + std::to_string(number), 1);
    }
  }

  TEST(QcMachineTest, RefusesToRunAFunctionTheHostNamesThatThereIsNot)
  {
    ProgsBuilder builder;
    EndMain(builder);
    QcMachine machine = MakeMachine(builder);

    EXPECT_FALSE(machine.Call("missing"));
    EXPECT_THAT(machine.GetError().message, HasSubstr("There is no function named missing"));
    EXPECT_EQ(machine.GetError().statement, -1);

    for (const std::int32_t number : {0, 2, -1})
    {
      EXPECT_FALSE(machine.Call(number));
      EXPECT_THAT(machine.GetError().message, HasSubstr("There is no function " + std::to_string(number)));
    }
  }

  TEST(QcMachineTest, StopsAtStateInAProgramWithoutWhatItNeeds)
  {
    ProgsBuilder builder;
    const std::uint16_t frame = builder.Float(1.0f);
    const std::uint16_t function = builder.Integer(1);
    builder.Emit(ProgsOpcode::State, frame, function);
    ExpectStopped(builder, "State needs the globals self and time", 1);
  }

  TEST(QcMachineTest, StopsAtStateWhenSelfIsNoEntity)
  {
    ProgsBuilder builder;
    builder.Field("nextthink", ProgsType::Float);
    builder.Field("frame", ProgsType::Float);
    builder.Field("think", ProgsType::Function);
    builder.Name(builder.Integer(4), "self", ProgsType::Entity);
    builder.Name(builder.Float(1.0f), "time", ProgsType::Float);
    const std::uint16_t frame = builder.Float(1.0f);
    const std::uint16_t function = builder.Integer(1);
    builder.Emit(ProgsOpcode::State, frame, function);
    ExpectStopped(builder, "There is no entity 4", 1);
  }

  TEST(QcMachineTest, IsStoppedByABuiltinWithAnErrorOfTheHost)
  {
    ProgsBuilder builder;
    const std::uint16_t function = builder.Integer(builder.Builtin("error", 10));
    const std::uint16_t one = builder.Float(1.0f);
    const std::uint16_t after = builder.Float();
    builder.Emit(ProgsOpcode::Call0, function);
    builder.Emit(ProgsOpcode::StoreF, one, after);
    EndMain(builder);
    QcMachine machine = MakeMachine(builder);
    machine.SetBuiltin(10, [](QcMachine &called)
    {
      called.Stop("The game gave up");
      // only the first error of a run is kept
      called.Stop("And again");
    });

    EXPECT_FALSE(machine.Call("main"));
    EXPECT_EQ(machine.GetError().message, "The game gave up");
    EXPECT_EQ(machine.GetError().function_name, "main");
    EXPECT_EQ(machine.GetError().statement, 1);
    EXPECT_EQ(machine.GetFloat(after), 0.0f);
  }

  TEST(QcMachineTest, StopsEveryRunUnderWayWhenOneInsideABuiltinFails)
  {
    ProgsBuilder builder;
    const std::uint16_t one = builder.Float(1.0f);
    const std::int32_t broken = builder.Emit(static_cast<ProgsOpcode>(200));
    builder.Function("broken", broken);
    const std::uint16_t function = builder.Integer(builder.Builtin("callback", 2));
    const std::uint16_t after = builder.Float();
    const std::int32_t main = builder.Emit(ProgsOpcode::Call0, function);
    builder.Emit(ProgsOpcode::StoreF, one, after);
    builder.Emit(ProgsOpcode::Done);
    builder.Function("main", main);
    QcMachine machine = MakeMachine(builder);

    bool inner = true;
    bool again = true;
    machine.SetBuiltin(2, [&inner, &again](QcMachine &called)
    {
      inner = called.Call("broken");
      // a run that was stopped runs nothing more
      again = called.Call("broken");
    });

    EXPECT_FALSE(machine.Call("main"));
    EXPECT_FALSE(inner);
    EXPECT_FALSE(again);
    EXPECT_THAT(machine.GetError().message, HasSubstr("Opcode 200 is not known"));
    EXPECT_EQ(machine.GetError().function_name, "broken");
    EXPECT_EQ(machine.GetError().statement, broken);
    EXPECT_EQ(machine.GetFloat(after), 0.0f);
  }

  TEST(QcMachineTest, RunsAgainAfterARunThatWasStopped)
  {
    ProgsBuilder builder;
    std::uint16_t n = 0;
    AddFactorial(builder, n);
    QcLimits limits;
    limits.stack_depth = 4;
    QcMachine machine = MakeMachine(builder, limits);

    machine.SetParameterFloat(0, 9.0f);
    EXPECT_FALSE(machine.Call("factorial"));
    EXPECT_TRUE(machine.HasFailed());

    machine.SetParameterFloat(0, 3.0f);
    EXPECT_TRUE(machine.Call("factorial"));
    EXPECT_FALSE(machine.HasFailed());
    EXPECT_TRUE(machine.GetError().message.empty());
    EXPECT_EQ(machine.GetReturnFloat(), 6.0f);
  }

  TEST(QcMachineTest, RunsAProgramThatWasNotReadFromAFileWithoutReadingOutsideIt)
  {
    // nothing checked these: no reserved globals, locals outside the
    // globals, a first statement that there is not
    Progs progs;
    progs.strings = std::string("\0main\0", 6);
    progs.statements.push_back({.opcode = 0, .a = 0, .b = 0, .c = 0});
    progs.functions.emplace_back();
    quake::ProgsFunction outside;
    outside.first_statement = 0;
    outside.first_local = 20;
    outside.locals_count = 100;
    outside.name = 1;
    progs.functions.push_back(outside);
    quake::ProgsFunction nowhere;
    nowhere.first_statement = 50;
    progs.functions.push_back(nowhere);
    quake::ProgsFunction parameters;
    parameters.parameters_count = 8;
    parameters.parameter_sizes.fill(200);
    parameters.locals_count = 4;
    progs.functions.push_back(parameters);
    QcMachine machine{std::move(progs)};

    EXPECT_EQ(machine.GetGlobalCount(), 28);
    EXPECT_FALSE(machine.Call(1));
    EXPECT_THAT(machine.GetError().message, HasSubstr("locals of function 1 (main) are outside the globals"));
    EXPECT_FALSE(machine.Call(2));
    EXPECT_THAT(machine.GetError().message, HasSubstr("went to statement 50"));
    EXPECT_FALSE(machine.Call(3));
    EXPECT_THAT(machine.GetError().message, HasSubstr("parameters of function 3 () do not fit its locals"));
  }
}
