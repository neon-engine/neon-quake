#include "entity-text.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "bsp-file.hpp"

namespace
{
  using quake::BspEntity;
  using quake::EntityText;
  using ::testing::ElementsAre;
  using ::testing::Pair;

  /// What reading a text says is wrong with it. Empty when nothing is.
  std::string ReasonOfRefusal(const std::string_view text)
  {
    EntityText entity_text;
    std::string error;
    if (entity_text.Read(text, error)) { return {}; }
    return error;
  }

  TEST(EntityTextTest, ReadsEveryEntityWithItsKeysAndValuesInOrder)
  {
    const std::string text =
      "{\n"
      "\"classname\" \"worldspawn\"\n"
      "\"wad\" \"gfx/base.wad\"\n"
      "\"message\" \"The Slipgate Complex\"\n"
      "}\n"
      "{\n"
      "\"origin\" \"480 -352 88\"\n"
      "\"classname\" \"info_player_start\"\n"
      "}\n";

    EntityText entity_text;
    std::string error;
    ASSERT_TRUE(entity_text.Read(text, error)) << error;

    ASSERT_EQ(entity_text.entities.size(), 2u);
    EXPECT_THAT(entity_text.entities[0].pairs, ElementsAre(
      Pair("classname", "worldspawn"),
      Pair("wad", "gfx/base.wad"),
      Pair("message", "The Slipgate Complex")));
    EXPECT_THAT(entity_text.entities[1].pairs, ElementsAre(
      Pair("origin", "480 -352 88"),
      Pair("classname", "info_player_start")));
  }

  TEST(EntityTextTest, FindsAValueByItsKey)
  {
    EntityText entity_text;
    std::string error;
    ASSERT_TRUE(entity_text.Read("{ \"classname\" \"light\" \"light\" \"300\" \"light\" \"200\" }", error)) << error;
    ASSERT_EQ(entity_text.entities.size(), 1u);

    const BspEntity &entity = entity_text.entities[0];
    ASSERT_NE(entity.Find("classname"), nullptr);
    EXPECT_EQ(*entity.Find("classname"), "light");

    // of a key written twice, the first
    ASSERT_NE(entity.Find("light"), nullptr);
    EXPECT_EQ(*entity.Find("light"), "300");

    EXPECT_EQ(entity.Find("origin"), nullptr);
    EXPECT_EQ(entity.Find("Classname"), nullptr);
  }

  TEST(EntityTextTest, ReadsBlocksWrittenTightOrSpreadOrEmpty)
  {
    EntityText entity_text;
    std::string error;
    ASSERT_TRUE(entity_text.Read("{\"a\"\"1\"}{}\r\n\t{\r\n\"b\"\n\n\"2\"\r\n}", error)) << error;

    ASSERT_EQ(entity_text.entities.size(), 3u);
    EXPECT_THAT(entity_text.entities[0].pairs, ElementsAre(Pair("a", "1")));
    EXPECT_TRUE(entity_text.entities[1].pairs.empty());
    EXPECT_THAT(entity_text.entities[2].pairs, ElementsAre(Pair("b", "2")));
  }

  TEST(EntityTextTest, KeepsWhatStandsBetweenQuotesAsItIs)
  {
    EntityText entity_text;
    std::string error;
    ASSERT_TRUE(entity_text.Read("{ \"message\" \"two\\nlines { } // kept\" \"\" \"\" }", error)) << error;

    ASSERT_EQ(entity_text.entities.size(), 1u);
    EXPECT_THAT(entity_text.entities[0].pairs, ElementsAre(
      Pair("message", "two\\nlines { } // kept"),
      Pair("", "")));
  }

  TEST(EntityTextTest, SkipsCommentsAndStopsAtAZero)
  {
    using namespace std::string_view_literals;

    EntityText entity_text;
    std::string error;
    ASSERT_TRUE(entity_text.Read("// the world\n{ \"a\" \"1\" // its only key\n}\n\0{ not read"sv, error)) << error;

    ASSERT_EQ(entity_text.entities.size(), 1u);
    EXPECT_THAT(entity_text.entities[0].pairs, ElementsAre(Pair("a", "1")));
  }

  TEST(EntityTextTest, MakesNothingOfNoText)
  {
    EntityText entity_text;
    std::string error;
    ASSERT_TRUE(entity_text.Read("", error)) << error;
    EXPECT_TRUE(entity_text.entities.empty());

    ASSERT_TRUE(entity_text.Read(" \n\t\n", error)) << error;
    EXPECT_TRUE(entity_text.entities.empty());
  }

  TEST(EntityTextTest, RefusesTextOutsideABlockAndStaysAsItWas)
  {
    EntityText entity_text;
    std::string error;
    ASSERT_TRUE(entity_text.Read("{ \"a\" \"1\" }", error)) << error;

    EXPECT_FALSE(entity_text.Read("{ \"b\" \"2\" }\n\"c\" \"3\"", error));
    EXPECT_EQ(error, "line 2, column 1: an entity starts with `{`, and `\"` was found");

    EXPECT_FALSE(entity_text.Read("{\n}\n  }", error));
    EXPECT_EQ(error, "line 3, column 3: an entity starts with `{`, and `}` was found");

    ASSERT_EQ(entity_text.entities.size(), 1u);
    EXPECT_THAT(entity_text.entities[0].pairs, ElementsAre(Pair("a", "1")));
  }

  TEST(EntityTextTest, RefusesAKeyThatIsNotInQuotes)
  {
    EXPECT_EQ(ReasonOfRefusal("{\nclassname \"light\"\n}"),
      "line 2, column 1: a key in quotes or `}` was expected, and `c` was found");

    // a block inside a block
    EXPECT_EQ(ReasonOfRefusal("{ \"a\" \"1\"\n { \"b\" \"2\" } }"),
      "line 2, column 2: a key in quotes or `}` was expected, and `{` was found");
  }

  TEST(EntityTextTest, RefusesAKeyWithoutAValue)
  {
    EXPECT_EQ(ReasonOfRefusal("{\n\"classname\"\n}"), "line 3, column 1: the key \"classname\" has no value in quotes");
    EXPECT_EQ(ReasonOfRefusal("{ \"light\" 300 }"), "line 1, column 11: the key \"light\" has no value in quotes");
    EXPECT_EQ(ReasonOfRefusal("{ \"light\""), "line 1, column 10: the key \"light\" has no value in quotes");
  }

  TEST(EntityTextTest, RefusesTextThatEndsInsideABlockOrInsideQuotes)
  {
    EXPECT_EQ(ReasonOfRefusal("{\n\"a\" \"1\"\n"),
      "line 3, column 1: the text ends inside the entity opened at line 1, column 1");
    EXPECT_EQ(ReasonOfRefusal("{"), "line 1, column 2: the text ends inside the entity opened at line 1, column 1");

    EXPECT_EQ(ReasonOfRefusal("{\n\"a\" \"1\n}\n"),
      "line 4, column 1: the text ends inside the quotes opened at line 2, column 5");
    EXPECT_EQ(ReasonOfRefusal("{ \"cla"),
      "line 1, column 7: the text ends inside the quotes opened at line 1, column 3");
  }

  TEST(EntityTextTest, ReadsTheEntitiesOfRealLevelsWhenTheyAreThere)
  {
    const std::filesystem::path maps = std::filesystem::path(QUAKE_TEST_DATA_DIRECTORY) / "maps";

    std::size_t read = 0;
    std::error_code ignored;
    for (std::filesystem::directory_iterator entry(maps, ignored), end; !ignored && entry != end;
      entry.increment(ignored))
    {
      if (entry->path().extension() != ".bsp") { continue; }

      std::ifstream stream(entry->path(), std::ios::binary);
      const std::vector<std::uint8_t> bytes(
        (std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

      // a level of another version is not what is read here
      quake::BspFile file;
      std::string error;
      if (!file.Read(bytes, error)) { continue; }

      EntityText entity_text;
      EXPECT_TRUE(entity_text.Read(file.entities, error)) << entry->path().string() << ": " << error;
      ASSERT_FALSE(entity_text.entities.empty()) << entry->path().string();
      ASSERT_NE(entity_text.entities[0].Find("classname"), nullptr) << entry->path().string();
      EXPECT_EQ(*entity_text.entities[0].Find("classname"), "worldspawn") << entry->path().string();
      read++;
    }
    if (read == 0) { GTEST_SKIP() << "No compiled level of version 29 in " << maps.string(); }
  }
}
