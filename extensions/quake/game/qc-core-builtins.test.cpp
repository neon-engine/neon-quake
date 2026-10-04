#include "qc-core-builtins.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "formats/progs-builder.test.hpp"
#include "qc-builtin-number.hpp"
#include "qc-recording-host.test.hpp"

namespace
{
  using quake::Progs;
  using quake::ProgsBuilder;
  using quake::ProgsOpcode;
  using quake::ProgsType;
  using quake::QcBuiltinNumber;
  using quake::QcCell;
  using quake::QcCoreBuiltins;
  using quake::QcLimits;
  using quake::QcMachine;
  using quake::QcRecordingHost;
  using ::testing::Contains;
  using ::testing::ElementsAre;
  using ::testing::FloatNear;
  using ::testing::HasSubstr;
  using ::testing::IsEmpty;

  using Vector = std::array<float, 3>;

  /// The builtins on a machine with a small program: the globals and fields
  /// they look for by name, and for every builtin a function that stands
  /// for it, which a test calls as the host with the parameters in place.
  class QcCoreBuiltinsTest : public ::testing::Test
  {
  protected:
    /// The highest number of a builtin of the original.
    static constexpr std::int32_t last_builtin = 78;

    ProgsBuilder _builder;
    QcRecordingHost _host;
    QcCoreBuiltins _builtins{_host};
    std::unique_ptr<QcMachine> _machine;

    // the globals of the program
    std::uint16_t _self = 0;
    std::uint16_t _message_entity = 0;
    std::uint16_t _forward = 0;
    std::uint16_t _right = 0;
    std::uint16_t _up = 0;

    // the fields of an entity
    std::uint16_t _classname = 0;
    std::uint16_t _model = 0;
    std::uint16_t _origin = 0;
    std::uint16_t _angles = 0;
    std::uint16_t _next_think = 0;
    std::uint16_t _solid = 0;
    std::uint16_t _health = 0;

    /// The number of the first function that stands for a builtin. There
    /// is one for every number of a builtin and every count of parameters,
    /// since the builtins that print take as many strings as they are
    /// called with.
    std::int32_t _first_function = 0;

    void SetUp() override
    {
      _self = _builder.Integer();
      _builder.Name(_self, "self", ProgsType::Entity);
      _message_entity = _builder.Integer();
      _builder.Name(_message_entity, "msg_entity", ProgsType::Entity);
      _forward = _builder.Vector();
      _builder.Name(_forward, "v_forward", ProgsType::Vector);
      _up = _builder.Vector();
      _builder.Name(_up, "v_up", ProgsType::Vector);
      _right = _builder.Vector();
      _builder.Name(_right, "v_right", ProgsType::Vector);

      _classname = _builder.Field("classname", ProgsType::String);
      _model = _builder.Field("model", ProgsType::String);
      _origin = _builder.Field("origin", ProgsType::Vector);
      _angles = _builder.Field("angles", ProgsType::Vector);
      _next_think = _builder.Field("nextthink", ProgsType::Float);
      _solid = _builder.Field("solid", ProgsType::Float);
      _health = _builder.Field("health", ProgsType::Float);

      _first_function = _builder.NextFunction();
      for (std::int32_t number = 1; number <= last_builtin; number++)
      {
        for (std::int32_t count = 0; count <= QcMachine::max_parameters; count++)
        {
          _builder.Builtin("", number);
          _builder.functions.back().parameters_count = count;
        }
      }

      MakeMachine();
    }

    /// Makes the machine of the program as the builder has it now, with the
    /// builtins on it.
    void MakeMachine(const QcLimits &limits = {})
    {
      Progs progs;
      std::string error;
      ASSERT_TRUE(progs.Read(_builder.Build(), error)) << error;
      _machine = std::make_unique<QcMachine>(std::move(progs), limits);
      _builtins.Register(*_machine);
    }

    /// Calls a builtin as the game code does with `count` parameters, which
    /// the test has set before. False when the run was stopped.
    bool Call(const QcBuiltinNumber number, const std::int32_t count)
    {
      const std::int32_t functions_of_one = QcMachine::max_parameters + 1;
      return _machine->Call(
        _first_function + (static_cast<std::int32_t>(number) - 1) * functions_of_one + count);
    }

    /// Calls a builtin of one number, and gives the number it returns.
    float CallWithFloat(const QcBuiltinNumber number, const float value)
    {
      _machine->SetParameterFloat(0, value);
      EXPECT_TRUE(Call(number, 1));
      return _machine->GetReturnFloat();
    }

    /// Calls a builtin of one vector.
    void CallWithVector(const QcBuiltinNumber number, const Vector &value)
    {
      _machine->SetParameterVector(0, value);
      EXPECT_TRUE(Call(number, 1));
    }

    /// Calls a builtin of one string.
    void CallWithString(const QcBuiltinNumber number, const std::string_view text)
    {
      _machine->SetParameterString(0, text);
      EXPECT_TRUE(Call(number, 1));
    }

    /// Makes an entity with `spawn`, as the game code does.
    std::int32_t Spawn()
    {
      EXPECT_TRUE(Call(QcBuiltinNumber::Spawn, 0));
      return _machine->GetReturnInteger();
    }

    void SetClassname(const std::int32_t entity, const std::string_view name) const
    {
      _machine->GetEntity(entity)[_classname] = QcCell::OfInteger(_machine->AddString(name));
    }

    /// The next entity `find` gives after one, for a classname.
    std::int32_t FindClassname(const std::int32_t start, const std::string_view name)
    {
      _machine->SetParameterInteger(0, start);
      _machine->SetParameterInteger(1, _classname);
      _machine->SetParameterString(2, name);
      EXPECT_TRUE(Call(QcBuiltinNumber::Find, 3));
      return _machine->GetReturnInteger();
    }
  };

  // Numbers and vectors.

  TEST_F(QcCoreBuiltinsTest, RoundsAndCutsNumbers)
  {
    EXPECT_EQ(CallWithFloat(QcBuiltinNumber::RInt, 2.5f), 3.0f);
    EXPECT_EQ(CallWithFloat(QcBuiltinNumber::RInt, 2.4f), 2.0f);
    // a half goes away from zero
    EXPECT_EQ(CallWithFloat(QcBuiltinNumber::RInt, -2.5f), -3.0f);
    EXPECT_EQ(CallWithFloat(QcBuiltinNumber::Floor, 2.9f), 2.0f);
    EXPECT_EQ(CallWithFloat(QcBuiltinNumber::Floor, -2.1f), -3.0f);
    EXPECT_EQ(CallWithFloat(QcBuiltinNumber::Ceil, 2.1f), 3.0f);
    EXPECT_EQ(CallWithFloat(QcBuiltinNumber::Ceil, -2.9f), -2.0f);
    EXPECT_EQ(CallWithFloat(QcBuiltinNumber::FAbs, -7.5f), 7.5f);
    EXPECT_EQ(CallWithFloat(QcBuiltinNumber::FAbs, 7.5f), 7.5f);
  }

  TEST_F(QcCoreBuiltinsTest, GivesTheLengthOfAVectorAndTheVectorOfLengthOne)
  {
    CallWithVector(QcBuiltinNumber::VLen, {3.0f, 4.0f, 12.0f});
    EXPECT_EQ(_machine->GetReturnFloat(), 13.0f);

    CallWithVector(QcBuiltinNumber::Normalize, {0.0f, 3.0f, 4.0f});
    EXPECT_THAT(_machine->GetReturnVector(), ElementsAre(0.0f, FloatNear(0.6f, 1e-6f), FloatNear(0.8f, 1e-6f)));

    // a vector of no length stays one
    CallWithVector(QcBuiltinNumber::Normalize, {});
    EXPECT_THAT(_machine->GetReturnVector(), ElementsAre(0.0f, 0.0f, 0.0f));
  }

  TEST_F(QcCoreBuiltinsTest, GivesTheYawOfADirectionInWholeDegrees)
  {
    const auto yaw = [this](const Vector &direction)
    {
      CallWithVector(QcBuiltinNumber::VecToYaw, direction);
      return _machine->GetReturnFloat();
    };

    EXPECT_EQ(yaw({1.0f, 0.0f, 0.0f}), 0.0f);
    EXPECT_EQ(yaw({1.0f, 1.0f, 0.0f}), 45.0f);
    EXPECT_EQ(yaw({0.0f, 1.0f, 0.0f}), 90.0f);
    EXPECT_EQ(yaw({-1.0f, 0.0f, 0.0f}), 180.0f);
    EXPECT_EQ(yaw({0.0f, -1.0f, 5.0f}), 270.0f);
    // 26.56 degrees: the part after the point is cut off, not rounded
    EXPECT_EQ(yaw({2.0f, 1.0f, 0.0f}), 26.0f);
    // -26.56 is cut to -26, and then counted from 0 upwards
    EXPECT_EQ(yaw({2.0f, -1.0f, 0.0f}), 334.0f);
    // straight up or down looks nowhere
    EXPECT_EQ(yaw({0.0f, 0.0f, 1.0f}), 0.0f);
  }

  TEST_F(QcCoreBuiltinsTest, GivesTheAnglesOfADirectionInWholeDegrees)
  {
    const auto angles = [this](const Vector &direction)
    {
      CallWithVector(QcBuiltinNumber::VecToAngles, direction);
      return _machine->GetReturnVector();
    };

    EXPECT_THAT(angles({1.0f, 0.0f, 0.0f}), ElementsAre(0.0f, 0.0f, 0.0f));
    EXPECT_THAT(angles({0.0f, 1.0f, 1.0f}), ElementsAre(45.0f, 90.0f, 0.0f));
    // downwards is counted from 360 back
    EXPECT_THAT(angles({1.0f, 0.0f, -1.0f}), ElementsAre(315.0f, 0.0f, 0.0f));
    EXPECT_THAT(angles({0.0f, 0.0f, 1.0f}), ElementsAre(90.0f, 0.0f, 0.0f));
    EXPECT_THAT(angles({0.0f, 0.0f, -1.0f}), ElementsAre(270.0f, 0.0f, 0.0f));
    EXPECT_THAT(angles({-3.0f, -3.0f, 0.0f}), ElementsAre(0.0f, 225.0f, 0.0f));
  }

  TEST_F(QcCoreBuiltinsTest, MakesTheThreeAxesOfAngles)
  {
    constexpr float tolerance = 1e-6f;
    const auto is = [](const float x, const float y, const float z)
    {
      return ElementsAre(FloatNear(x, tolerance), FloatNear(y, tolerance), FloatNear(z, tolerance));
    };

    // no turn: forward is x, right is minus y, up is z
    CallWithVector(QcBuiltinNumber::MakeVectors, {0.0f, 0.0f, 0.0f});
    EXPECT_THAT(_machine->GetVector(_forward), is(1.0f, 0.0f, 0.0f));
    EXPECT_THAT(_machine->GetVector(_right), is(0.0f, -1.0f, 0.0f));
    EXPECT_THAT(_machine->GetVector(_up), is(0.0f, 0.0f, 1.0f));

    // a yaw of 90 degrees turns to the left
    CallWithVector(QcBuiltinNumber::MakeVectors, {0.0f, 90.0f, 0.0f});
    EXPECT_THAT(_machine->GetVector(_forward), is(0.0f, 1.0f, 0.0f));
    EXPECT_THAT(_machine->GetVector(_right), is(1.0f, 0.0f, 0.0f));
    EXPECT_THAT(_machine->GetVector(_up), is(0.0f, 0.0f, 1.0f));

    // a positive pitch looks down
    CallWithVector(QcBuiltinNumber::MakeVectors, {90.0f, 0.0f, 0.0f});
    EXPECT_THAT(_machine->GetVector(_forward), is(0.0f, 0.0f, -1.0f));
    EXPECT_THAT(_machine->GetVector(_right), is(0.0f, -1.0f, 0.0f));
    EXPECT_THAT(_machine->GetVector(_up), is(1.0f, 0.0f, 0.0f));

    // a roll of 90 degrees tips the right side down
    CallWithVector(QcBuiltinNumber::MakeVectors, {0.0f, 0.0f, 90.0f});
    EXPECT_THAT(_machine->GetVector(_forward), is(1.0f, 0.0f, 0.0f));
    EXPECT_THAT(_machine->GetVector(_right), is(0.0f, 0.0f, -1.0f));
    EXPECT_THAT(_machine->GetVector(_up), is(0.0f, -1.0f, 0.0f));

    // all three at once: the axes stay at right angles and of length one
    CallWithVector(QcBuiltinNumber::MakeVectors, {20.0f, 130.0f, -35.0f});
    const Vector forward = _machine->GetVector(_forward);
    const Vector right = _machine->GetVector(_right);
    const Vector up = _machine->GetVector(_up);
    const auto dot = [](const Vector &a, const Vector &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; };
    EXPECT_NEAR(dot(forward, forward), 1.0f, tolerance);
    EXPECT_NEAR(dot(right, right), 1.0f, tolerance);
    EXPECT_NEAR(dot(up, up), 1.0f, tolerance);
    EXPECT_NEAR(dot(forward, right), 0.0f, tolerance);
    EXPECT_NEAR(dot(forward, up), 0.0f, tolerance);
    EXPECT_NEAR(dot(right, up), 0.0f, tolerance);
  }

  TEST_F(QcCoreBuiltinsTest, GivesRandomNumbersBetweenZeroAndOneThatASeedRepeats)
  {
    const auto take = [this]
    {
      std::array<float, 200> numbers{};
      for (float &number : numbers)
      {
        EXPECT_TRUE(Call(QcBuiltinNumber::Random, 0));
        number = _machine->GetReturnFloat();
        EXPECT_GT(number, 0.0f);
        EXPECT_LT(number, 1.0f);
      }
      return numbers;
    };

    _builtins.SeedRandom(7);
    const std::array<float, 200> first = take();
    const std::array<float, 200> second = take();
    EXPECT_NE(first, second);

    _builtins.SeedRandom(7);
    EXPECT_EQ(take(), first);

    _builtins.SeedRandom(8);
    EXPECT_NE(take(), first);

    // they are spread over the range, not all of one half
    float sum = 0.0f;
    for (const float number : first) { sum += number; }
    EXPECT_NEAR(sum / 200.0f, 0.5f, 0.1f);
  }

  // Text.

  TEST_F(QcCoreBuiltinsTest, WritesANumberWithoutAFractionWhenItHasNone)
  {
    const auto text = [this](const float value)
    {
      CallWithFloat(QcBuiltinNumber::FToS, value);
      return _machine->GetReturnString();
    };

    EXPECT_EQ(text(25.0f), "25");
    EXPECT_EQ(text(-3.0f), "-3");
    EXPECT_EQ(text(0.0f), "0");
    EXPECT_EQ(text(-0.0f), "0");
    EXPECT_EQ(text(100000.0f), "100000");
    // one digit of the fraction, in five places at the least
    EXPECT_EQ(text(2.5f), "  2.5");
    EXPECT_EQ(text(-0.25f), " -0.2");
    EXPECT_EQ(text(1234.56f), "1234.6");
  }

  TEST_F(QcCoreBuiltinsTest, WritesAVectorInQuotesWithOneDigitOfEachFraction)
  {
    CallWithVector(QcBuiltinNumber::VToS, {1.0f, -20.5f, 300.25f});
    EXPECT_EQ(_machine->GetReturnString(), "'  1.0 -20.5 300.2'");
  }

  TEST_F(QcCoreBuiltinsTest, HandsWhatIsPrintedToTheHostEachKindToItsOwn)
  {
    CallWithString(QcBuiltinNumber::BPrint, "a grunt died\n");
    CallWithString(QcBuiltinNumber::DPrint, "the door is stuck\n");

    _machine->SetParameterInteger(0, 1);
    _machine->SetParameterString(1, "You got the shells\n");
    EXPECT_TRUE(Call(QcBuiltinNumber::SPrint, 2));

    _machine->SetParameterInteger(0, 2);
    _machine->SetParameterString(1, "The gate opens");
    EXPECT_TRUE(Call(QcBuiltinNumber::CenterPrint, 2));

    EXPECT_THAT(_host.calls, ElementsAre(
      "bprint: a grunt died\n",
      "dprint: the door is stuck\n",
      "sprint 1: You got the shells\n",
      "centerprint 2: The gate opens"));
  }

  TEST_F(QcCoreBuiltinsTest, PrintsATextThatIsPassedInPiecesAsOne)
  {
    _machine->SetParameterString(0, "player");
    _machine->SetParameterString(1, " was shot by ");
    _machine->SetParameterString(2, "a grunt\n");
    EXPECT_TRUE(Call(QcBuiltinNumber::BPrint, 3));

    // only as many pieces as the call has: the third is left from before
    _machine->SetParameterString(0, "2 ");
    _machine->SetParameterString(1, "secrets\n");
    EXPECT_TRUE(Call(QcBuiltinNumber::DPrint, 2));

    _machine->SetParameterInteger(0, 1);
    _machine->SetParameterString(1, "Only ");
    _machine->SetParameterString(2, "3");
    _machine->SetParameterString(3, " more to go");
    EXPECT_TRUE(Call(QcBuiltinNumber::CenterPrint, 4));

    EXPECT_THAT(_host.calls, ElementsAre(
      "bprint: player was shot by a grunt\n", "dprint: 2 secrets\n", "centerprint 1: Only 3 more to go"));
  }

  TEST_F(QcCoreBuiltinsTest, AsksTheHostToPrintAnEntityAndWithCoredumpEveryOneThatIsNotFree)
  {
    const std::int32_t first = Spawn();
    const std::int32_t second = Spawn();
    _machine->SetParameterInteger(0, first);
    EXPECT_TRUE(Call(QcBuiltinNumber::Remove, 1));
    _host.calls.clear();

    _machine->SetParameterInteger(0, second);
    EXPECT_TRUE(Call(QcBuiltinNumber::EPrint, 1));
    EXPECT_THAT(_host.calls, ElementsAre("eprint 2"));
    _host.calls.clear();

    EXPECT_TRUE(Call(QcBuiltinNumber::CoreDump, 0));
    EXPECT_THAT(_host.calls, ElementsAre("eprint 0", "eprint 2"));
  }

  TEST_F(QcCoreBuiltinsTest, StopsTheRunWithAnErrorOfTheGameCode)
  {
    const std::int32_t entity = Spawn();
    _host.calls.clear();
    _machine->SetInteger(_self, entity);
    _machine->SetParameterString(0, "no spot ");
    _machine->SetParameterString(1, "to start at");

    EXPECT_FALSE(Call(QcBuiltinNumber::Error, 2));
    EXPECT_TRUE(_machine->HasFailed());
    EXPECT_EQ(_machine->GetError().message, "The game code says: no spot to start at");
    EXPECT_TRUE(_machine->GetError().message.starts_with(QcCoreBuiltins::error_start));
    EXPECT_THAT(_host.calls, ElementsAre("error 1: no spot to start at"));
    // the entity it ran for stays
    EXPECT_FALSE(_machine->IsEntityFree(entity));
  }

  TEST_F(QcCoreBuiltinsTest, RemovesTheEntityOfAnObjectErrorAndGoesOn)
  {
    Spawn();
    const std::int32_t entity = Spawn();
    _host.calls.clear();
    _machine->SetInteger(_self, entity);

    CallWithString(QcBuiltinNumber::ObjError, "a door without a target");
    EXPECT_FALSE(_machine->HasFailed());
    EXPECT_TRUE(_machine->IsEntityFree(entity));
    EXPECT_FALSE(_machine->IsEntityFree(1));
    EXPECT_THAT(_host.calls, ElementsAre("objerror 2: a door without a target", "removed 2"));
  }

  TEST_F(QcCoreBuiltinsTest, GoesOnAfterWhatTheOriginalHadForADebugger)
  {
    EXPECT_TRUE(Call(QcBuiltinNumber::Break, 0));
    EXPECT_TRUE(Call(QcBuiltinNumber::TraceOn, 0));
    EXPECT_TRUE(Call(QcBuiltinNumber::TraceOff, 0));
    EXPECT_THAT(_host.calls, IsEmpty());
  }

  // The entities.

  TEST_F(QcCoreBuiltinsTest, MakesAnEntityAndTellsTheHost)
  {
    EXPECT_EQ(Spawn(), 1);
    EXPECT_EQ(Spawn(), 2);
    EXPECT_EQ(_machine->GetEntityCount(), 3);
    EXPECT_FALSE(_machine->IsEntityFree(2));
    EXPECT_THAT(_host.calls, ElementsAre("made 1", "made 2"));
  }

  TEST_F(QcCoreBuiltinsTest, StopsTheRunWhenThereIsNoRoomForAnotherEntity)
  {
    MakeMachine({.entities = 2});
    EXPECT_EQ(Spawn(), 1);

    EXPECT_FALSE(Call(QcBuiltinNumber::Spawn, 0));
    EXPECT_THAT(_machine->GetError().message, HasSubstr("no room"));
    EXPECT_THAT(_host.calls, ElementsAre("made 1"));
  }

  TEST_F(QcCoreBuiltinsTest, RemovesAnEntityResetsWhatItShowsAndTellsTheHost)
  {
    const std::int32_t entity = Spawn();
    const std::span<QcCell> fields = _machine->GetEntity(entity);
    fields[_model] = QcCell::OfInteger(_machine->AddString("progs/soldier.mdl"));
    fields[_origin + 2] = QcCell::OfFloat(64.0f);
    fields[_angles + 1] = QcCell::OfFloat(90.0f);
    fields[_next_think] = QcCell::OfFloat(3.5f);
    fields[_solid] = QcCell::OfFloat(2.0f);
    fields[_health] = QcCell::OfFloat(30.0f);
    _host.calls.clear();

    _machine->SetParameterInteger(0, entity);
    EXPECT_TRUE(Call(QcBuiltinNumber::Remove, 1));

    EXPECT_TRUE(_machine->IsEntityFree(entity));
    EXPECT_THAT(_host.calls, ElementsAre("removed 1"));
    // out of the world, and it thinks no more
    EXPECT_EQ(fields[_model].AsInteger(), 0);
    EXPECT_EQ(fields[_origin + 2].AsFloat(), 0.0f);
    EXPECT_EQ(fields[_angles + 1].AsFloat(), 0.0f);
    EXPECT_EQ(fields[_solid].AsFloat(), 0.0f);
    EXPECT_EQ(fields[_next_think].AsFloat(), -1.0f);
    // the rest stays, for the game code that goes on with it
    EXPECT_EQ(fields[_health].AsFloat(), 30.0f);
  }

  TEST_F(QcCoreBuiltinsTest, RemovesNeitherTheWorldNorAnEntityTwice)
  {
    const std::int32_t entity = Spawn();
    _host.calls.clear();

    _machine->SetParameterInteger(0, 0);
    EXPECT_TRUE(Call(QcBuiltinNumber::Remove, 1));
    EXPECT_FALSE(_machine->IsEntityFree(0));

    _machine->SetParameterInteger(0, entity);
    EXPECT_TRUE(Call(QcBuiltinNumber::Remove, 1));
    EXPECT_TRUE(Call(QcBuiltinNumber::Remove, 1));

    _machine->SetParameterInteger(0, 99);
    EXPECT_TRUE(Call(QcBuiltinNumber::Remove, 1));
    EXPECT_THAT(_host.calls, ElementsAre("removed 1"));
  }

  TEST_F(QcCoreBuiltinsTest, LetsTheHostRemoveAnEntityTheSameWay)
  {
    const std::int32_t entity = Spawn();
    _machine->GetEntity(entity)[_next_think] = QcCell::OfFloat(2.0f);
    _host.calls.clear();

    EXPECT_TRUE(_builtins.RemoveEntity(*_machine, entity));
    EXPECT_TRUE(_machine->IsEntityFree(entity));
    EXPECT_EQ(_machine->GetEntity(entity)[_next_think].AsFloat(), -1.0f);
    EXPECT_FALSE(_builtins.RemoveEntity(*_machine, entity));
    EXPECT_FALSE(_builtins.RemoveEntity(*_machine, 0));
    EXPECT_THAT(_host.calls, ElementsAre("removed 1"));
  }

  TEST_F(QcCoreBuiltinsTest, FindsTheEntitiesWhoseFieldHasAText)
  {
    for (int i = 0; i < 5; i++) { Spawn(); }
    SetClassname(1, "door");
    SetClassname(2, "light");
    SetClassname(3, "door");
    SetClassname(4, "door");
    SetClassname(5, "light");
    _machine->SetParameterInteger(0, 3);
    EXPECT_TRUE(Call(QcBuiltinNumber::Remove, 1));

    // from the world on, each after the one before, and the world at the end
    EXPECT_EQ(FindClassname(0, "door"), 1);
    // the third is free, and is left out though it still has its classname
    EXPECT_EQ(FindClassname(1, "door"), 4);
    EXPECT_EQ(FindClassname(4, "door"), 0);
    EXPECT_EQ(FindClassname(0, "light"), 2);
    EXPECT_EQ(FindClassname(2, "light"), 5);
    EXPECT_EQ(FindClassname(5, "light"), 0);
    EXPECT_EQ(FindClassname(0, "monster"), 0);
    // texts are compared whole
    EXPECT_EQ(FindClassname(0, "doo"), 0);
  }

  TEST_F(QcCoreBuiltinsTest, FindsNothingForAFieldThereIsNotOrAStartThereIsNot)
  {
    Spawn();
    SetClassname(1, "door");

    _machine->SetParameterInteger(0, 0);
    _machine->SetParameterInteger(1, 1000);
    _machine->SetParameterString(2, "door");
    EXPECT_TRUE(Call(QcBuiltinNumber::Find, 3));
    EXPECT_EQ(_machine->GetReturnInteger(), 0);

    _machine->SetParameterInteger(1, -1);
    EXPECT_TRUE(Call(QcBuiltinNumber::Find, 3));
    EXPECT_EQ(_machine->GetReturnInteger(), 0);

    EXPECT_EQ(FindClassname(1000, "door"), 0);
    // a start before the world is the world
    EXPECT_EQ(FindClassname(-5, "door"), 1);
  }

  TEST_F(QcCoreBuiltinsTest, GivesTheNextEntityThatIsNotFreeAndTheWorldAtTheEnd)
  {
    for (int i = 0; i < 4; i++) { Spawn(); }
    _machine->SetParameterInteger(0, 2);
    EXPECT_TRUE(Call(QcBuiltinNumber::Remove, 1));
    _machine->SetParameterInteger(0, 4);
    EXPECT_TRUE(Call(QcBuiltinNumber::Remove, 1));

    const auto next = [this](const std::int32_t entity)
    {
      _machine->SetParameterInteger(0, entity);
      EXPECT_TRUE(Call(QcBuiltinNumber::NextEnt, 1));
      return _machine->GetReturnInteger();
    };
    EXPECT_EQ(next(0), 1);
    EXPECT_EQ(next(1), 3);
    // the last one is free, so there is none after the third
    EXPECT_EQ(next(3), 0);
    EXPECT_EQ(next(4), 0);
    EXPECT_EQ(next(1000), 0);
  }

  // The files, the console variables, the lights.

  TEST_F(QcCoreBuiltinsTest, KeepsTheModelsAndSoundsTheGameCodeNamesAndReturnsEachName)
  {
    // the host names the level first
    EXPECT_EQ(_builtins.GetModels().Add("maps/start.bsp"), 1);

    CallWithString(QcBuiltinNumber::PrecacheModel, "progs/player.mdl");
    EXPECT_EQ(_machine->GetReturnString(), "progs/player.mdl");
    EXPECT_EQ(_machine->GetReturnInteger(), _machine->GetParameterInteger(0));
    CallWithString(QcBuiltinNumber::PrecacheModel2, "progs/hknight.mdl");
    CallWithString(QcBuiltinNumber::PrecacheModel, "progs/player.mdl");

    CallWithString(QcBuiltinNumber::PrecacheSound, "weapons/rocket1i.wav");
    EXPECT_EQ(_machine->GetReturnString(), "weapons/rocket1i.wav");
    CallWithString(QcBuiltinNumber::PrecacheSound2, "knight/hit.wav");

    // a file that is neither is only given back
    CallWithString(QcBuiltinNumber::PrecacheFile, "gfx/menu.lmp");
    EXPECT_EQ(_machine->GetReturnString(), "gfx/menu.lmp");
    CallWithString(QcBuiltinNumber::PrecacheFile2, "gfx/pop.lmp");
    EXPECT_EQ(_machine->GetReturnString(), "gfx/pop.lmp");

    EXPECT_THAT(
      _builtins.GetModels().GetNames(), ElementsAre("", "maps/start.bsp", "progs/player.mdl", "progs/hknight.mdl"));
    EXPECT_EQ(_builtins.GetModels().Find("progs/player.mdl"), 2);
    EXPECT_THAT(_builtins.GetSounds().GetNames(), ElementsAre("", "weapons/rocket1i.wav", "knight/hit.wav"));
  }

  TEST_F(QcCoreBuiltinsTest, ReadsAndSetsTheConsoleVariables)
  {
    const auto read = [this](const std::string_view name)
    {
      CallWithString(QcBuiltinNumber::CVar, name);
      return _machine->GetReturnFloat();
    };

    EXPECT_EQ(read("sv_gravity"), 800.0f);
    EXPECT_EQ(read("no_such_variable"), 0.0f);

    // what the host set
    _builtins.GetVariables().Set("skill", "3");
    EXPECT_EQ(read("skill"), 3.0f);

    // what the game code sets, the host reads
    _machine->SetParameterString(0, "sv_gravity");
    _machine->SetParameterString(1, "100");
    EXPECT_TRUE(Call(QcBuiltinNumber::CVarSet, 2));
    EXPECT_EQ(read("sv_gravity"), 100.0f);
    EXPECT_EQ(_builtins.GetVariables().GetText("sv_gravity"), "100");
  }

  TEST_F(QcCoreBuiltinsTest, KeepsTheStylesOfTheLightsAndTellsTheHost)
  {
    const auto set = [this](const float style, const std::string_view text)
    {
      _machine->SetParameterFloat(0, style);
      _machine->SetParameterString(1, text);
      EXPECT_TRUE(Call(QcBuiltinNumber::LightStyle, 2));
    };

    EXPECT_EQ(_builtins.GetLightStyle(0), "");
    set(0.0f, "m");
    set(1.0f, "mmnmmommommnonmmonqnmmo");
    set(63.0f, "a");
    set(1.0f, "mmamammmmammamamaaamammma");
    EXPECT_EQ(_builtins.GetLightStyle(0), "m");
    EXPECT_EQ(_builtins.GetLightStyle(1), "mmamammmmammamamaaamammma");
    EXPECT_EQ(_builtins.GetLightStyle(63), "a");
    EXPECT_EQ(_builtins.GetLightStyle(2), "");
    EXPECT_EQ(_host.calls.size(), 4u);
    EXPECT_THAT(_host.calls, Contains("lightstyle 63: a"));

    // a style there is not is left out
    set(64.0f, "z");
    set(-1.0f, "z");
    set(std::nanf(""), "z");
    EXPECT_EQ(_host.calls.size(), 4u);
    EXPECT_EQ(_builtins.GetLightStyle(64), "");
    EXPECT_EQ(_builtins.GetLightStyle(-1), "");
  }

  TEST_F(QcCoreBuiltinsTest, TakesTheStyleOfALightFromASavedGameAndTellsTheHost)
  {
    EXPECT_TRUE(_builtins.RestoreLightStyle(32, "a"));
    EXPECT_EQ(_builtins.GetLightStyle(32), "a");
    EXPECT_THAT(_host.calls, Contains("lightstyle 32: a"));

    EXPECT_FALSE(_builtins.RestoreLightStyle(64, "z"));
    EXPECT_FALSE(_builtins.RestoreLightStyle(-1, "z"));
    EXPECT_EQ(_host.calls.size(), 1u);
  }

  // What is the host's alone to do.

  TEST_F(QcCoreBuiltinsTest, HandsCommandsAndTheChangeOfLevelToTheHost)
  {
    _machine->SetParameterInteger(0, 1);
    _machine->SetParameterString(1, "bf\n");
    EXPECT_TRUE(Call(QcBuiltinNumber::StuffCmd, 2));
    CallWithString(QcBuiltinNumber::LocalCmd, "restart\n");
    CallWithString(QcBuiltinNumber::ChangeLevel, "e1m2");
    _machine->SetParameterInteger(0, 1);
    EXPECT_TRUE(Call(QcBuiltinNumber::SetSpawnParms, 1));

    EXPECT_THAT(_host.calls, ElementsAre(
      "stuffcmd 1: bf\n", "localcmd: restart\n", "changelevel: e1m2", "setspawnparms 1"));
  }

  TEST_F(QcCoreBuiltinsTest, HandsThePartsOfAMessageToTheHostEachAsItsKind)
  {
    const auto write = [this](const QcBuiltinNumber number, const float destination, const float value)
    {
      _machine->SetParameterFloat(0, destination);
      _machine->SetParameterFloat(1, value);
      EXPECT_TRUE(Call(number, 2));
    };
    _machine->SetInteger(_message_entity, 4);

    write(QcBuiltinNumber::WriteByte, 2.0f, 30.0f);
    write(QcBuiltinNumber::WriteChar, 0.0f, -3.0f);
    write(QcBuiltinNumber::WriteShort, 3.0f, 1000.0f);
    write(QcBuiltinNumber::WriteLong, 2.0f, 70000.0f);
    // to one player: the one the global names
    write(QcBuiltinNumber::WriteCoord, 1.0f, 12.5f);
    write(QcBuiltinNumber::WriteAngle, 1.0f, 90.0f);

    _machine->SetParameterFloat(0, 2.0f);
    _machine->SetParameterString(1, "e1m2");
    EXPECT_TRUE(Call(QcBuiltinNumber::WriteString, 2));

    _machine->SetParameterFloat(0, 0.0f);
    _machine->SetParameterInteger(1, 7);
    EXPECT_TRUE(Call(QcBuiltinNumber::WriteEntity, 2));

    EXPECT_THAT(_host.calls, ElementsAre(
      "write to 2 for 0: byte 30",
      "write to 0 for 0: char -3",
      "write to 3 for 0: short 1000",
      "write to 2 for 0: long 70000",
      "write to 1 for 4: coord 12.5",
      "write to 1 for 4: angle 90",
      "write to 2 for 0: string e1m2",
      "write to 0 for 0: entity 7"));
  }

  TEST_F(QcCoreBuiltinsTest, StopsTheRunForAMessageToADestinationThereIsNot)
  {
    _machine->SetParameterFloat(0, 4.0f);
    _machine->SetParameterFloat(1, 1.0f);
    EXPECT_FALSE(Call(QcBuiltinNumber::WriteByte, 2));
    EXPECT_THAT(_machine->GetError().message, HasSubstr("destination 4"));

    _machine->SetParameterFloat(0, 0.5f);
    EXPECT_FALSE(Call(QcBuiltinNumber::WriteByte, 2));
    EXPECT_THAT(_host.calls, IsEmpty());
  }

  // The game code itself.

  TEST_F(QcCoreBuiltinsTest, IsCalledByStatementsOfTheGameCode)
  {
    // void() main =
    // {
    //   made = spawn();
    //   made.classname = "rocket";
    //   bprint(ftos(vlen('3 4 0')), " units\n");
    //   found = find(world, classname, "rocket");
    //   remove(found);
    // };
    const auto function_of = [this](const QcBuiltinNumber number)
    {
      return _builder.Integer(_builder.Builtin(quake::QcBuiltinName(number), static_cast<std::int32_t>(number)));
    };
    const std::uint16_t spawn = function_of(QcBuiltinNumber::Spawn);
    const std::uint16_t vlen = function_of(QcBuiltinNumber::VLen);
    const std::uint16_t ftos = function_of(QcBuiltinNumber::FToS);
    const std::uint16_t bprint = function_of(QcBuiltinNumber::BPrint);
    const std::uint16_t find = function_of(QcBuiltinNumber::Find);
    const std::uint16_t remove = function_of(QcBuiltinNumber::Remove);

    const std::uint16_t world = _builder.Integer(0);
    const std::uint16_t made = _builder.Integer();
    const std::uint16_t found = _builder.Integer();
    const std::uint16_t classname = _builder.Integer(_classname);
    const std::uint16_t rocket = _builder.StringGlobal("rocket");
    const std::uint16_t units = _builder.StringGlobal(" units\n");
    const std::uint16_t distance = _builder.Vector(3.0f, 4.0f, 0.0f);
    const std::uint16_t pointer = _builder.Integer();

    const auto returned = static_cast<std::uint16_t>(QcMachine::return_offset);
    const auto parameter = [](const std::int32_t number)
    {
      return static_cast<std::uint16_t>(QcMachine::first_parameter_offset + number * QcMachine::parameter_size);
    };

    const std::int32_t start = _builder.Emit(ProgsOpcode::Call0, spawn);
    _builder.Emit(ProgsOpcode::StoreEnt, returned, made);
    _builder.Emit(ProgsOpcode::Address, made, classname, pointer);
    _builder.Emit(ProgsOpcode::StorepS, rocket, pointer);
    _builder.Emit(ProgsOpcode::StoreV, distance, parameter(0));
    _builder.Emit(ProgsOpcode::Call1, vlen);
    _builder.Emit(ProgsOpcode::StoreF, returned, parameter(0));
    _builder.Emit(ProgsOpcode::Call1, ftos);
    _builder.Emit(ProgsOpcode::StoreS, returned, parameter(0));
    _builder.Emit(ProgsOpcode::StoreS, units, parameter(1));
    _builder.Emit(ProgsOpcode::Call2, bprint);
    _builder.Emit(ProgsOpcode::StoreEnt, world, parameter(0));
    _builder.Emit(ProgsOpcode::StoreFld, classname, parameter(1));
    _builder.Emit(ProgsOpcode::StoreS, rocket, parameter(2));
    _builder.Emit(ProgsOpcode::Call3, find);
    _builder.Emit(ProgsOpcode::StoreEnt, returned, found);
    _builder.Emit(ProgsOpcode::StoreEnt, found, parameter(0));
    _builder.Emit(ProgsOpcode::Call1, remove);
    _builder.Emit(ProgsOpcode::Done);
    _builder.Function("main", start);
    MakeMachine();

    ASSERT_TRUE(_machine->Call("main")) << _machine->GetError().message;
    EXPECT_EQ(_machine->GetInteger(made), 1);
    EXPECT_EQ(_machine->GetInteger(found), 1);
    EXPECT_TRUE(_machine->IsEntityFree(1));
    EXPECT_THAT(_host.calls, ElementsAre("made 1", "bprint: 5 units\n", "removed 1"));
  }
}
