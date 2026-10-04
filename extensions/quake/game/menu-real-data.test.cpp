#include "menu.hpp"

#include <set>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "formats/picture-reader.hpp"
#include "formats/real-data.test.hpp"
#include "formats/wad.hpp"

namespace
{
  using quake::HudPicture;
  using quake::HudPictureKind;
  using quake::Menu;
  using quake::MenuAction;
  using quake::MenuActionKind;
  using quake::MenuGame;
  using quake::MenuKey;
  using quake::MenuOptions;
  using quake::MenuScreen;
  using quake::Picture;
  using quake::RealData;
  using quake::Wad;

  /// Goes through the menus, every screen with its cursor on every item
  /// and at every picture of the turning cursor, and gathers the names of
  /// all pictures drawn and of all sounds asked for.
  class Walk final
  {
    Menu _menu;
    MenuGame _game;
    MenuOptions _options;

    void Draw()
    {
      for (int tenth = 0; tenth < 6; tenth++)
      {
        for (const HudPicture &picture : _menu.Layout(0.05 + 0.1 * tenth, _options, _game))
        {
          pictures.emplace(picture.name);
          if (picture.kind == HudPictureKind::Character)
          {
            EXPECT_EQ(picture.name, HudPicture::characters_name);
            EXPECT_GE(picture.character, 0);
            EXPECT_LT(picture.character, 256);
          }
        }
      }
    }

    void Note(const std::vector<MenuAction> &actions)
    {
      for (const MenuAction &action : actions)
      {
        if (action.kind == MenuActionKind::PlaySound) { sounds.emplace(action.name); }
        if (action.kind == MenuActionKind::SetOption) { EXPECT_TRUE(_options.Set(action.name, action.value)); }
      }
    }

    void Press(const MenuKey key)
    {
      Note(_menu.Press(key, _options, _game));
      Draw();
    }

  public:
    std::set<std::string> pictures;
    std::set<std::string> sounds;

    Walk()
    {
      _game.is_running = true;
      _game.slots[2] = "a saved game";

      Note(_menu.Open());
      Draw();

      // each screen of the main menu, with every arrow many times
      for (int item = 0; item < Menu::main_items; item++)
      {
        Press(MenuKey::Select);
        for (const MenuKey key : {MenuKey::Down, MenuKey::Right, MenuKey::Up, MenuKey::Left})
        {
          for (int i = 0; i < 13; i++) { Press(key); }
        }
        Press(MenuKey::Back);
        EXPECT_EQ(_menu.GetScreen(), MenuScreen::Main);
        Press(MenuKey::Down);
      }

      // the question, and the slots to load and to save
      Press(MenuKey::Select);
      Press(MenuKey::Select);
      EXPECT_EQ(_menu.GetScreen(), MenuScreen::NewGame);
      Press(MenuKey::Back);
      for (const MenuScreen screen : {MenuScreen::Load, MenuScreen::Save})
      {
        Press(MenuKey::Down);
        Press(MenuKey::Select);
        EXPECT_EQ(_menu.GetScreen(), screen);
        for (int i = 0; i < 13; i++) { Press(MenuKey::Down); }
        Press(MenuKey::Back);
      }

      for (const HudPicture &picture : Menu::LayoutPause()) { pictures.emplace(picture.name); }
    }
  };

  TEST(MenuRealDataTest, DrawsOnlyPicturesAndPlaysOnlySoundsTheRealDataHas)
  {
    const RealData &data = RealData::Get();
    if (!data.IsThere()) { GTEST_SKIP() << data.GetProblem(); }

    Wad wad;
    std::string error;
    ASSERT_TRUE(wad.Read(data.GetBytes("gfx.wad"), error)) << error;

    const Walk walk;

    // the plaque, 6 titles, 3 pictures of items, 6 of the cursor, 6 pages
    // of help, 10 pieces of the box, the pause, and the letters
    EXPECT_EQ(walk.pictures.size(), 34u);
    for (const std::string &name : walk.pictures)
    {
      Picture picture;
      if (name.starts_with("gfx/"))
      {
        const auto bytes = data.GetBytes(name);
        ASSERT_FALSE(bytes.empty()) << "the paks have no " << name;
        EXPECT_TRUE(quake::read_picture_file(name, bytes, picture, error)) << name << ": " << error;
      }
      else
      {
        ASSERT_NE(wad.Find(name), nullptr) << "gfx.wad has no " << name;
        EXPECT_TRUE(wad.ReadPicture(name, picture, error)) << name << ": " << error;
      }
      EXPECT_GT(picture.width, 0) << name;
      EXPECT_GT(picture.height, 0) << name;
      EXPECT_LE(picture.width, 320) << name;
      EXPECT_LE(picture.height, 200) << name;
    }

    EXPECT_THAT(walk.sounds, ::testing::ElementsAre("misc/menu1.wav", "misc/menu2.wav", "misc/menu3.wav"));
    for (const std::string &name : walk.sounds)
    {
      EXPECT_FALSE(data.GetBytes("sound/" + name).empty()) << "the paks have no sound/" << name;
    }
  }

  TEST(MenuRealDataTest, FindsThePiecesOfTheBoxOfTheSizesTheLayoutCountsOn)
  {
    const RealData &data = RealData::Get();
    if (!data.IsThere()) { GTEST_SKIP() << data.GetProblem(); }

    const auto expect_size = [&](const std::string &name, const std::int32_t width, const std::int32_t height)
    {
      Picture picture;
      std::string error;
      ASSERT_TRUE(quake::read_picture_file(name, data.GetBytes(name), picture, error)) << name << ": " << error;
      EXPECT_EQ(picture.width, width) << name;
      EXPECT_EQ(picture.height, height) << name;
    };

    for (const char *side : {"tl", "ml", "bl", "tr", "mr", "br"})
    {
      expect_size(std::string("gfx/box_") + side + ".lmp", 8, 8);
    }
    for (const char *middle : {"tm", "mm", "mm2", "bm"})
    {
      expect_size(std::string("gfx/box_") + middle + ".lmp", 16, 8);
    }
    for (int page = 0; page < Menu::help_pages; page++)
    {
      expect_size("gfx/help" + std::to_string(page) + ".lmp", 320, 200);
    }
  }
}
