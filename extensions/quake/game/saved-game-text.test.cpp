#include "saved-game-text.hpp"

#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{
  using quake::SavedGame;
  using quake::SavedGameEntity;
  using quake::SavedGameText;
  using ::testing::HasSubstr;

  /// A saved game made by hand: a level with the world, a thing, a free
  /// entity, and a sign.
  SavedGame MakeGame()
  {
    SavedGame game;
    game.comment = SavedGameText::MakeComment("the Test Level", 3, 42);
    for (std::size_t i = 0; i < game.parms.size(); i++) { game.parms[i] = static_cast<float>(i) * 0.5f; }
    game.skill = 2;
    game.map_name = "e1m1";
    game.time = 12.5;
    game.light_styles[0] = "m";
    game.light_styles[1] = "mmnmmommommnonmmonqnmmo";
    game.light_styles[32] = "a";
    game.globals = {{"self", "1"}, {"time", "12.500000"}, {"mapname", "e1m1"}};
    game.entities = {
      SavedGameEntity{.free = false, .pairs = {{"classname", "worldspawn"}}},
      SavedGameEntity{.free = false, .pairs = {{"classname", "thing"}, {"origin", "1.000000 2.500000 -3.000000"}}},
      SavedGameEntity{.free = true, .pairs = {}},
      SavedGameEntity{.free = false, .pairs = {{"message", "two\nlines"}}},
    };
    return game;
  }

  /// The lines of the head of MakeGame(), up to the globals.
  std::string MakeHead()
  {
    std::string head = "5\nthe_Test_Level________kills:__3/_42____\n";
    for (int i = 0; i < 16; i++) { head += i % 2 == 0 ? std::to_string(i / 2) + ".000000\n" : std::to_string(i / 2) + ".500000\n"; }
    head += "2\ne1m1\n12.500000\n";
    for (int i = 0; i < 64; i++)
    {
      head += i == 1 ? "mmnmmommommnonmmonqnmmo\n" : i == 32 ? "a\n" : "m\n";
    }
    return head;
  }

  void ExpectEqual(const SavedGame &a, const SavedGame &b)
  {
    EXPECT_EQ(a.version, b.version);
    EXPECT_EQ(a.comment, b.comment);
    EXPECT_EQ(a.parms, b.parms);
    EXPECT_EQ(a.skill, b.skill);
    EXPECT_EQ(a.map_name, b.map_name);
    EXPECT_EQ(a.time, b.time);
    EXPECT_EQ(a.light_styles, b.light_styles);
    EXPECT_EQ(a.globals, b.globals);
    ASSERT_EQ(a.entities.size(), b.entities.size());
    for (std::size_t i = 0; i < a.entities.size(); i++)
    {
      EXPECT_EQ(a.entities[i].free, b.entities[i].free) << i;
      EXPECT_EQ(a.entities[i].pairs, b.entities[i].pairs) << i;
    }
  }

  TEST(SavedGameTextTest, WritesTheTextOfTheOriginal)
  {
    const std::string expected = MakeHead() +
      "{\n"
      "\"self\" \"1\"\n"
      "\"time\" \"12.500000\"\n"
      "\"mapname\" \"e1m1\"\n"
      "}\n"
      "{\n"
      "\"classname\" \"worldspawn\"\n"
      "}\n"
      "{\n"
      "\"classname\" \"thing\"\n"
      "\"origin\" \"1.000000 2.500000 -3.000000\"\n"
      "}\n"
      "{\n"
      "}\n"
      "{\n"
      "\"message\" \"two\nlines\"\n"
      "}\n";
    EXPECT_EQ(SavedGameText::Write(MakeGame()), expected);
  }

  TEST(SavedGameTextTest, ReadsBackWhatItWrote)
  {
    SavedGame written = MakeGame();
    SavedGame read;
    std::string error;
    ASSERT_TRUE(SavedGameText::Read(SavedGameText::Write(written), read, error)) << error;

    // a style that was never set comes back as the light as it is
    for (std::string &style : written.light_styles)
    {
      if (style.empty()) { style = "m"; }
    }
    ExpectEqual(read, written);
  }

  TEST(SavedGameTextTest, WritesWhatCouldNotBeReadBackInAnotherWay)
  {
    SavedGame game = MakeGame();
    game.comment = "";
    game.map_name = "my level";
    game.light_styles[5] = "a b\n";
    game.globals = {{"mapname", "say \"hello\""}};
    game.entities[1].pairs = {{"message", std::string(2000, 'x')}};

    SavedGame read;
    std::string error;
    ASSERT_TRUE(SavedGameText::Read(SavedGameText::Write(game), read, error)) << error;
    EXPECT_EQ(read.comment, "_");
    EXPECT_EQ(read.map_name, "my_level");
    EXPECT_EQ(read.light_styles[5], "ab");
    EXPECT_EQ(read.globals[0].second, "say 'hello'");
    EXPECT_EQ(read.entities[1].pairs[0].second, std::string(SavedGameText::max_token_length, 'x'));
  }

  TEST(SavedGameTextTest, TakesAnEntityWithoutFieldsAsFree)
  {
    SavedGame game = MakeGame();
    game.entities[3].pairs.clear();

    SavedGame read;
    std::string error;
    ASSERT_TRUE(SavedGameText::Read(SavedGameText::Write(game), read, error)) << error;
    ASSERT_EQ(read.entities.size(), 4u);
    EXPECT_FALSE(read.entities[1].free);
    EXPECT_TRUE(read.entities[2].free);
    EXPECT_TRUE(read.entities[3].free);
  }

  TEST(SavedGameTextTest, ReadsLineEndsOfWindowsAndASkillThatIsAFloat)
  {
    std::string text = SavedGameText::Write(MakeGame());
    const std::size_t skill = text.find("\n2\ne1m1\n");
    ASSERT_NE(skill, std::string::npos);
    text.replace(skill, 3, "\n2.000000\n");

    std::string windows;
    for (const char letter : text) { windows += letter == '\n' ? std::string("\r\n") : std::string(1, letter); }

    SavedGame read;
    std::string error;
    ASSERT_TRUE(SavedGameText::Read(windows, read, error)) << error;
    EXPECT_EQ(read.skill, 2);
    EXPECT_EQ(read.map_name, "e1m1");
    EXPECT_EQ(read.entities[3].pairs[0].second, "two\nlines");
  }

  TEST(SavedGameTextTest, LeavesOutWhatAnEngineOfTodayAddsAtTheEnd)
  {
    const std::string text = SavedGameText::Write(MakeGame()) +
      "/*\n// QuakeSpasm extended savegame\nsv.model_precache 1 \"maps/e1m1.bsp\"\n*/\n";

    SavedGame read;
    std::string error;
    ASSERT_TRUE(SavedGameText::Read(text, read, error)) << error;
    EXPECT_EQ(read.entities.size(), 4u);
  }

  TEST(SavedGameTextTest, RefusesAnotherVersion)
  {
    std::string text = SavedGameText::Write(MakeGame());
    text[0] = '6';

    SavedGame read;
    read.map_name = "kept";
    std::string error;
    EXPECT_FALSE(SavedGameText::Read(text, read, error));
    EXPECT_THAT(error, HasSubstr("version 6"));
    EXPECT_EQ(read.map_name, "kept");

    EXPECT_FALSE(SavedGameText::Read("", read, error));
    EXPECT_THAT(error, HasSubstr("the version"));
    EXPECT_FALSE(SavedGameText::Read("five\n", read, error));
    EXPECT_THAT(error, HasSubstr("no number"));
  }

  TEST(SavedGameTextTest, RefusesAHeadThatEndsEarlyOrHasNoNumberWhereOneIsDue)
  {
    SavedGame read;
    std::string error;
    EXPECT_FALSE(SavedGameText::Read("5\ncomment\n1.0\n2.0\n", read, error));
    EXPECT_THAT(error, HasSubstr("parm 3"));

    std::string text = SavedGameText::Write(MakeGame());
    const std::size_t time = text.find("12.500000");
    text.replace(time, 9, "noon");
    EXPECT_FALSE(SavedGameText::Read(text, read, error));
    EXPECT_THAT(error, HasSubstr("`noon` for the time"));

    EXPECT_FALSE(SavedGameText::Read(MakeHead().substr(0, MakeHead().size() - 4), read, error));
    EXPECT_THAT(error, HasSubstr("light style"));

    EXPECT_FALSE(SavedGameText::Read("5\n" + std::string(2000, 'c') + "\n", read, error));
    EXPECT_THAT(error, HasSubstr("the comment"));
  }

  TEST(SavedGameTextTest, RefusesABlockThatDoesNotCloseAndANameWithoutAValue)
  {
    SavedGame read;
    std::string error;
    EXPECT_FALSE(SavedGameText::Read(MakeHead(), read, error));
    EXPECT_THAT(error, HasSubstr("no block of globals"));

    EXPECT_FALSE(SavedGameText::Read(MakeHead() + "{\n\"self\" \"1\"\n", read, error));
    EXPECT_THAT(error, HasSubstr("ends inside"));

    EXPECT_FALSE(SavedGameText::Read(MakeHead() + "{\n}\n{\n\"classname\"\n}\n", read, error));
    EXPECT_THAT(error, HasSubstr("has no value"));
    // the line is the one of the file, where the value was looked for
    EXPECT_THAT(error, HasSubstr("line 90"));

    EXPECT_FALSE(SavedGameText::Read(MakeHead() + "{\n}\n{\n\"classname\" \"thing\n}\n", read, error));
    EXPECT_THAT(error, HasSubstr("inside the quotes"));

    EXPECT_FALSE(SavedGameText::Read(MakeHead() + "{\n}\nclassname\n", read, error));
    EXPECT_THAT(error, HasSubstr("starts with `{`"));
  }

  TEST(SavedGameTextTest, RefusesWhatIsPastItsLimits)
  {
    SavedGame read;
    std::string error;

    std::string many_pairs = MakeHead() + "{\n";
    for (std::size_t i = 0; i <= SavedGameText::max_pairs; i++) { many_pairs += "\"a\" \"1\"\n"; }
    EXPECT_FALSE(SavedGameText::Read(many_pairs + "}\n", read, error));
    EXPECT_THAT(error, HasSubstr("pairs"));

    std::string many_entities = MakeHead() + "{\n}\n";
    for (std::size_t i = 0; i <= SavedGameText::max_entities; i++) { many_entities += "{\n}\n"; }
    EXPECT_FALSE(SavedGameText::Read(many_entities, read, error));
    EXPECT_THAT(error, HasSubstr("entities"));

    const std::string long_value = MakeHead() + "{\n\"a\" \"" + std::string(1024, 'x') + "\"\n}\n";
    EXPECT_FALSE(SavedGameText::Read(long_value, read, error));
    EXPECT_THAT(error, HasSubstr("more than 1023 bytes"));

    const std::string huge(SavedGameText::max_text_size + 1, ' ');
    EXPECT_FALSE(SavedGameText::Read(huge, read, error));
    EXPECT_THAT(error, HasSubstr("bytes"));
  }

  TEST(SavedGameTextTest, MakesTheCommentOfTheOriginal)
  {
    EXPECT_EQ(SavedGameText::MakeComment("the Test Level", 3, 42), "the_Test_Level________kills:__3/_42____");
    EXPECT_EQ(SavedGameText::MakeComment("", 0, 0), "______________________kills:__0/__0____");
    // a long name is cut where the kills start, and long counts where the comment ends
    EXPECT_EQ(
      SavedGameText::MakeComment("a name that is far too long for it", 123, 456),
      "a_name_that_is_far_tookills:123/456____");
    EXPECT_EQ(SavedGameText::MakeComment("x", 123456, 1234567).size(), SavedGameText::comment_length);
    EXPECT_EQ(SavedGameText::MakeComment("two\nlines", 1, 2), "two_lines_____________kills:__1/__2____");
  }
} // namespace
