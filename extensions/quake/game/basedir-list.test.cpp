#include "basedir-list.hpp"

#include <map>

#include <gtest/gtest.h>

namespace
{
  using quake::Basedir;
  using quake::BasedirList;

  /// The folders in each folder of `assets://basedirs/`, as a file system
  /// would list them.
  class BasedirListTest : public ::testing::Test
  {
  protected:
    std::map<std::string, std::vector<std::string>> _folders;

    [[nodiscard]] std::vector<Basedir> Find(const std::vector<std::string> &top) const
    {
      return BasedirList::Find(top, [this](const std::string &folder)
      {
        const auto found = _folders.find(folder);
        return found == _folders.end() ? std::vector<std::string>{} : found->second;
      });
    }
  };

  TEST_F(BasedirListTest, FindsEveryFolderThatHoldsAnId1InTheOrderOfTheirNames)
  {
    _folders["assets://basedirs/steam/"] = {"id1", "rogue"};
    _folders["assets://basedirs/librequake/"] = {"id1"};

    const auto found = Find({"steam", "librequake"});

    ASSERT_EQ(found.size(), 2u);
    EXPECT_EQ(found[0].name, "librequake");
    EXPECT_EQ(found[0].data_folder, "assets://basedirs/librequake/id1/");
    EXPECT_EQ(found[0].UserFolder(), "user://librequake/");
    EXPECT_EQ(found[1].name, "steam");
    EXPECT_EQ(found[1].data_folder, "assets://basedirs/steam/id1/");
  }

  TEST_F(BasedirListTest, LeavesOutAFolderWithoutAnId1)
  {
    _folders["assets://basedirs/steam/"] = {"id1"};
    _folders["assets://basedirs/notes/"] = {"pictures"};

    const auto found = Find({"notes", "steam"});

    ASSERT_EQ(found.size(), 1u);
    EXPECT_EQ(found[0].name, "steam");
  }

  TEST_F(BasedirListTest, FindsAnId1InAnyCaseAndKeepsTheCaseItHas)
  {
    _folders["assets://basedirs/steam/"] = {"ID1"};

    const auto found = Find({"steam"});

    ASSERT_EQ(found.size(), 1u);
    EXPECT_EQ(found[0].data_folder, "assets://basedirs/steam/ID1/");
  }

  TEST_F(BasedirListTest, TakesAnId1StraightUnderTheFolderAsTheOnlyOne)
  {
    _folders["assets://basedirs/steam/"] = {"id1"};

    const auto found = Find({"id1", "steam"});

    ASSERT_EQ(found.size(), 1u);
    EXPECT_EQ(found[0].name, "default");
    EXPECT_EQ(found[0].data_folder, "assets://basedirs/id1/");
    EXPECT_EQ(found[0].UserFolder(), "user://default/");
  }

  TEST_F(BasedirListTest, FindsNothingWhereThereIsNothing)
  {
    EXPECT_TRUE(Find({}).empty());
  }
} // namespace
