#include "qc-value-text.hpp"

#include <array>
#include <cmath>
#include <limits>
#include <string>

#include <gtest/gtest.h>

#include "saved-game-program.test.hpp"

namespace
{
  using quake::ProgsType;
  using quake::QcCell;
  using quake::QcMachine;
  using quake::QcValueText;
  using quake::SavedGameProgram;

  class QcValueTextTest : public ::testing::Test
  {
  protected:
    SavedGameProgram _program;
    QcMachine _machine = _program.Make();

    /// The text of what a text was made into.
    std::string Through(const ProgsType type, const std::string_view text)
    {
      QcValueText::Cells cells;
      EXPECT_TRUE(QcValueText::Parse(_machine, type, text, cells)) << text;
      return QcValueText::Print(_machine, type, cells);
    }
  };

  TEST_F(QcValueTextTest, PrintsAFloatWithSixPlacesAsTheOriginal)
  {
    const auto print = [this](const float value)
    {
      return QcValueText::Print(_machine, ProgsType::Float, std::array{QcCell::OfFloat(value)});
    };
    EXPECT_EQ(print(0.0f), "0.000000");
    EXPECT_EQ(print(100.0f), "100.000000");
    EXPECT_EQ(print(-0.5f), "-0.500000");
    EXPECT_EQ(print(0.1f), "0.100000");
    EXPECT_EQ(print(-0.0f), "-0.000000");
    EXPECT_EQ(print(1e-9f), "0.000000");
    EXPECT_EQ(print(std::numeric_limits<float>::max()), "340282346638528859811704183484516925440.000000");
  }

  TEST_F(QcValueTextTest, PrintsAVectorAsThreeFloats)
  {
    const std::array cells = {QcCell::OfFloat(1.0f), QcCell::OfFloat(2.5f), QcCell::OfFloat(-3.0f)};
    EXPECT_EQ(QcValueText::Print(_machine, ProgsType::Vector, cells), "1.000000 2.500000 -3.000000");

    // too few cells are no vector
    EXPECT_EQ(QcValueText::Print(_machine, ProgsType::Vector, std::span(cells).first(2)), "");
  }

  TEST_F(QcValueTextTest, TakesEveryKindFromItsTextAndGivesItBack)
  {
    EXPECT_EQ(Through(ProgsType::Float, "12.25"), "12.250000");
    EXPECT_EQ(Through(ProgsType::Float, "not a number"), "0.000000");
    EXPECT_EQ(Through(ProgsType::Vector, "1 -2 3.5"), "1.000000 -2.000000 3.500000");
    EXPECT_EQ(Through(ProgsType::Vector, "4 5"), "4.000000 5.000000 0.000000");
    EXPECT_EQ(Through(ProgsType::String, "a text"), "a text");
    EXPECT_EQ(Through(ProgsType::String, "two\\nlines"), "two\nlines");
    EXPECT_EQ(Through(ProgsType::String, "two\nlines"), "two\nlines");
    EXPECT_EQ(Through(ProgsType::Entity, "17"), "17");
    EXPECT_EQ(Through(ProgsType::Function, "thing_think"), "thing_think");
    EXPECT_EQ(Through(ProgsType::Field, "health"), "health");
    // the field, not its first part, which is at the same place
    EXPECT_EQ(Through(ProgsType::Field, "origin"), "origin");
    EXPECT_EQ(Through(ProgsType::Field, "origin_x"), "origin");
  }

  TEST_F(QcValueTextTest, RefusesWhatTheProgramDoesNotHaveAndLeavesTheCells)
  {
    QcValueText::Cells cells = {QcCell::OfInteger(5), QcCell::OfInteger(6), QcCell::OfInteger(7)};
    EXPECT_FALSE(QcValueText::Parse(_machine, ProgsType::Function, "no_such_function", cells));
    EXPECT_FALSE(QcValueText::Parse(_machine, ProgsType::Field, "no_such_field", cells));
    EXPECT_FALSE(QcValueText::Parse(_machine, ProgsType::Void, "void", cells));
    EXPECT_FALSE(QcValueText::Parse(_machine, ProgsType::Pointer, "1", cells));
    EXPECT_EQ(cells[0].AsInteger(), 5);
    EXPECT_EQ(cells[2].AsInteger(), 7);
  }

  TEST_F(QcValueTextTest, PrintsNothingForWhatThereIsNot)
  {
    const auto print = [this](const ProgsType type, const std::int32_t value)
    {
      return QcValueText::Print(_machine, type, std::array{QcCell::OfInteger(value)});
    };
    EXPECT_EQ(print(ProgsType::Function, 1000), "");
    EXPECT_EQ(print(ProgsType::Function, -1), "");
    EXPECT_EQ(print(ProgsType::Field, 1000), "");
    EXPECT_EQ(print(ProgsType::String, 100000), "");
    EXPECT_EQ(print(ProgsType::Void, 1), "");
    EXPECT_EQ(print(ProgsType::Pointer, 1), "");
  }

  TEST_F(QcValueTextTest, SaysHowManyCellsAValueIs)
  {
    EXPECT_EQ(QcValueText::GetSize(ProgsType::Vector), 3u);
    EXPECT_EQ(QcValueText::GetSize(ProgsType::Float), 1u);
    EXPECT_EQ(QcValueText::GetSize(ProgsType::String), 1u);
  }
} // namespace
