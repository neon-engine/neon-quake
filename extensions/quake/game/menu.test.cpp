#include "menu.hpp"

#include <limits>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "hud-picture.test.hpp"

namespace quake
{
  /// How a test that fails writes a `MenuAction`.
  inline void PrintTo(const MenuAction &action, std::ostream *stream)
  {
    *stream << "action " << static_cast<int>(action.kind) << " '" << action.name << "' value " << action.value
      << " slot " << action.slot;
  }
}

namespace
{
  using quake::HudAnchor;
  using quake::HudPicture;
  using quake::HudPictureKind;
  using quake::MakeLetter;
  using quake::MakeLetters;
  using quake::MakePicture;
  using quake::Menu;
  using quake::MenuAction;
  using quake::MenuActionKind;
  using quake::MenuGame;
  using quake::MenuKey;
  using quake::MenuOptions;
  using quake::MenuScreen;
  using quake::MenuTitleWidths;
  using ::testing::ElementsAre;
  using ::testing::ElementsAreArray;
  using ::testing::IsEmpty;

  constexpr HudAnchor center = HudAnchor::Center;

  constexpr MenuKey all_keys[] = {
    MenuKey::Up, MenuKey::Down, MenuKey::Left, MenuKey::Right,
    MenuKey::Select, MenuKey::Back, MenuKey::Yes, MenuKey::No,
  };

  const MenuAction move_sound = {.kind = MenuActionKind::PlaySound, .name = "misc/menu1.wav"};
  const MenuAction enter_sound = {.kind = MenuActionKind::PlaySound, .name = "misc/menu2.wav"};
  const MenuAction change_sound = {.kind = MenuActionKind::PlaySound, .name = "misc/menu3.wav"};
  const MenuAction resume = {.kind = MenuActionKind::Resume};
  const MenuAction new_game = {.kind = MenuActionKind::NewGame};
  const MenuAction quit = {.kind = MenuActionKind::Quit};

  MenuAction set_option(const std::string_view name, const float value)
  {
    return {.kind = MenuActionKind::SetOption, .name = name, .value = value};
  }

  /// A game that runs, with a saved game in the slots 0 and 3.
  MenuGame make_running_game()
  {
    MenuGame game;
    game.is_running = true;
    game.slots[0] = "the Slipgate Complex kills: 12/ 34";
    game.slots[3] = "e1m2";
    return game;
  }

  /// The same letters in bronze, for what a test expects.
  std::vector<HudPicture> make_bronze_letters(const std::string_view text, const std::int32_t x, const std::int32_t y)
  {
    std::vector<HudPicture> letters = MakeLetters(text, x, y, center);
    for (HudPicture &letter : letters) { letter.character += 128; }
    return letters;
  }

  void append(std::vector<HudPicture> &to, const std::vector<HudPicture> &more)
  {
    to.insert(to.end(), more.begin(), more.end());
  }

  /// A menu with the cursor of the main menu moved down so many times.
  Menu make_menu_at(const int main_item)
  {
    Menu menu;
    menu.Open();
    for (int i = 0; i < main_item; i++) { menu.Press(MenuKey::Down); }
    return menu;
  }

  /// A menu on the screen that an item of the main menu leads to.
  Menu make_menu_in(const int main_item)
  {
    Menu menu = make_menu_at(main_item);
    menu.Press(MenuKey::Select);
    return menu;
  }

  /// A menu on the screen that an item of the single player screen leads
  /// to, while a game runs.
  Menu make_menu_in_single_player(const int item)
  {
    Menu menu = make_menu_in(0);
    for (int i = 0; i < item; i++) { menu.Press(MenuKey::Down); }
    menu.Press(MenuKey::Select, {}, make_running_game());
    return menu;
  }

  /// The slider of the option in a row of the options screen: the x of
  /// its knob.
  std::int32_t find_knob(const std::vector<HudPicture> &pictures, const std::int32_t y)
  {
    for (const HudPicture &picture : pictures)
    {
      if (picture.kind == HudPictureKind::Character && picture.character == 131 && picture.y == y)
      {
        return picture.x;
      }
    }
    return -1;
  }

  // ---- closed and open

  TEST(MenuTest, StartsClosedAndDrawsAndDoesNothing)
  {
    Menu menu;
    EXPECT_FALSE(menu.IsOpen());
    EXPECT_EQ(menu.GetScreen(), MenuScreen::None);
    EXPECT_THAT(menu.Layout(1.0), IsEmpty());
    for (const MenuKey key : all_keys) { EXPECT_THAT(menu.Press(key), IsEmpty()); }
    EXPECT_FALSE(menu.IsOpen());
  }

  TEST(MenuTest, OpensTheMainMenuWithItsSound)
  {
    Menu menu;
    EXPECT_THAT(menu.Open(), ElementsAre(enter_sound));
    EXPECT_TRUE(menu.IsOpen());
    EXPECT_EQ(menu.GetScreen(), MenuScreen::Main);

    // an open one stays where it is
    menu.Press(MenuKey::Select);
    EXPECT_THAT(menu.Open(), IsEmpty());
    EXPECT_EQ(menu.GetScreen(), MenuScreen::SinglePlayer);

    menu.Close();
    EXPECT_FALSE(menu.IsOpen());
    EXPECT_THAT(menu.Layout(1.0), IsEmpty());
  }

  // ---- the main menu

  TEST(MenuTest, LaysOutTheMainMenuWithTheCursorOnEachItem)
  {
    Menu menu;
    menu.Open();
    for (int item = 0; item < 5; item++)
    {
      EXPECT_EQ(menu.GetCursor(), item);
      EXPECT_THAT(menu.Layout(0.0), ElementsAre(
        MakePicture("gfx/qplaque.lmp", 16, 4, center),
        MakePicture("gfx/ttl_main.lmp", 112, 4, center),
        MakePicture("gfx/mainmenu.lmp", 72, 32, center),
        MakePicture("gfx/menudot1.lmp", 54, 32 + item * 20, center))) << item;

      EXPECT_THAT(menu.Press(MenuKey::Down), ElementsAre(move_sound));
    }

    // around, to the first
    EXPECT_EQ(menu.GetCursor(), 0);
    EXPECT_THAT(menu.Press(MenuKey::Up), ElementsAre(move_sound));
    EXPECT_EQ(menu.GetCursor(), 4);
    EXPECT_THAT(menu.Press(MenuKey::Up), ElementsAre(move_sound));
    EXPECT_EQ(menu.GetCursor(), 3);
  }

  TEST(MenuTest, TurnsTheCursorSixPicturesAtTenASecond)
  {
    Menu menu;
    menu.Open();
    const auto cursor_at = [&](const double time) { return menu.Layout(time).back().name; };

    EXPECT_EQ(cursor_at(0.0), "gfx/menudot1.lmp");
    EXPECT_EQ(cursor_at(0.09), "gfx/menudot1.lmp");
    EXPECT_EQ(cursor_at(0.11), "gfx/menudot2.lmp");
    EXPECT_EQ(cursor_at(0.25), "gfx/menudot3.lmp");
    EXPECT_EQ(cursor_at(0.35), "gfx/menudot4.lmp");
    EXPECT_EQ(cursor_at(0.45), "gfx/menudot5.lmp");
    EXPECT_EQ(cursor_at(0.55), "gfx/menudot6.lmp");
    EXPECT_EQ(cursor_at(0.65), "gfx/menudot1.lmp");
    EXPECT_EQ(cursor_at(1234.75), "gfx/menudot6.lmp");

    // a time that is none is the start
    EXPECT_EQ(cursor_at(-3.0), "gfx/menudot1.lmp");
    EXPECT_EQ(cursor_at(std::numeric_limits<double>::quiet_NaN()), "gfx/menudot1.lmp");
    EXPECT_EQ(cursor_at(std::numeric_limits<double>::infinity()), "gfx/menudot1.lmp");
  }

  TEST(MenuTest, KeepsATitleOfAnotherWidthInTheMiddle)
  {
    MenuTitleWidths widths;
    widths.main = 156;
    widths.single_player = 70;
    widths.multiplayer = 100;
    widths.load = 72;
    widths.save = 74;
    widths.options = 114;

    EXPECT_EQ(make_menu_at(0).Layout(0.0, {}, {}, widths)[1], MakePicture("gfx/ttl_main.lmp", 82, 4, center));
    EXPECT_EQ(make_menu_in(0).Layout(0.0, {}, {}, widths)[1], MakePicture("gfx/ttl_sgl.lmp", 125, 4, center));
    EXPECT_EQ(make_menu_in(1).Layout(0.0, {}, {}, widths)[1], MakePicture("gfx/p_multi.lmp", 110, 4, center));
    EXPECT_EQ(make_menu_in(2).Layout(0.0, {}, {}, widths)[1], MakePicture("gfx/p_option.lmp", 103, 4, center));
    EXPECT_EQ(
      make_menu_in_single_player(1).Layout(0.0, {}, {}, widths)[0], MakePicture("gfx/p_load.lmp", 124, 4, center));
    EXPECT_EQ(
      make_menu_in_single_player(2).Layout(0.0, {}, {}, widths)[0], MakePicture("gfx/p_save.lmp", 123, 4, center));
  }

  TEST(MenuTest, GoesFromTheMainMenuIntoEachScreen)
  {
    constexpr MenuScreen screens[] = {
      MenuScreen::SinglePlayer, MenuScreen::Multiplayer, MenuScreen::Options, MenuScreen::Help, MenuScreen::Quit,
    };
    for (int item = 0; item < 5; item++)
    {
      Menu menu = make_menu_at(item);
      EXPECT_THAT(menu.Press(MenuKey::Select), ElementsAre(enter_sound)) << item;
      EXPECT_EQ(menu.GetScreen(), screens[item]) << item;
    }
  }

  TEST(MenuTest, ClosesFromTheMainMenuWithBack)
  {
    Menu menu = make_menu_at(2);
    EXPECT_THAT(menu.Press(MenuKey::Back), ElementsAre(resume));
    EXPECT_FALSE(menu.IsOpen());

    // the cursor is where it was left
    menu.Open();
    EXPECT_EQ(menu.GetCursor(), 2);
  }

  TEST(MenuTest, DoesNothingOnTheMainMenuForTheOtherKeys)
  {
    Menu menu = make_menu_at(1);
    for (const MenuKey key : {MenuKey::Left, MenuKey::Right, MenuKey::Yes, MenuKey::No})
    {
      EXPECT_THAT(menu.Press(key), IsEmpty());
      EXPECT_EQ(menu.GetScreen(), MenuScreen::Main);
      EXPECT_EQ(menu.GetCursor(), 1);
    }
  }

  // ---- single player

  TEST(MenuTest, LaysOutTheSinglePlayerScreen)
  {
    Menu menu = make_menu_in(0);
    menu.Press(MenuKey::Down);
    menu.Press(MenuKey::Down);

    EXPECT_THAT(menu.Layout(0.2), ElementsAre(
      MakePicture("gfx/qplaque.lmp", 16, 4, center),
      MakePicture("gfx/ttl_sgl.lmp", 96, 4, center),
      MakePicture("gfx/sp_menu.lmp", 72, 32, center),
      MakePicture("gfx/menudot3.lmp", 54, 72, center)));
  }

  TEST(MenuTest, MovesAroundTheThreeItemsOfSinglePlayer)
  {
    Menu menu = make_menu_in(0);
    EXPECT_THAT(menu.Press(MenuKey::Up), ElementsAre(move_sound));
    EXPECT_EQ(menu.GetCursor(), 2);
    EXPECT_THAT(menu.Press(MenuKey::Down), ElementsAre(move_sound));
    EXPECT_EQ(menu.GetCursor(), 0);
    EXPECT_THAT(menu.Press(MenuKey::Down), ElementsAre(move_sound));
    EXPECT_EQ(menu.GetCursor(), 1);

    for (const MenuKey key : {MenuKey::Left, MenuKey::Right, MenuKey::Yes, MenuKey::No})
    {
      EXPECT_THAT(menu.Press(key), IsEmpty());
    }
    EXPECT_EQ(menu.GetScreen(), MenuScreen::SinglePlayer);

    EXPECT_THAT(menu.Press(MenuKey::Back), ElementsAre(enter_sound));
    EXPECT_EQ(menu.GetScreen(), MenuScreen::Main);
  }

  TEST(MenuTest, StartsANewGameAtOnceWhenNoneRuns)
  {
    Menu menu = make_menu_in(0);
    EXPECT_THAT(menu.Press(MenuKey::Select), ElementsAre(enter_sound, new_game));
    EXPECT_FALSE(menu.IsOpen());
  }

  TEST(MenuTest, AsksBeforeANewGameEndsTheOneThatRuns)
  {
    const MenuGame game = make_running_game();

    Menu menu = make_menu_in(0);
    EXPECT_THAT(menu.Press(MenuKey::Select, {}, game), ElementsAre(enter_sound));
    EXPECT_EQ(menu.GetScreen(), MenuScreen::NewGame);

    // two lines, each around the middle, in white
    std::vector<HudPicture> expected = MakeLetters("Are you sure you want to", 64, 70, center);
    append(expected, MakeLetters("start a new game?", 92, 78, center));
    EXPECT_THAT(menu.Layout(0.0, {}, game), ElementsAreArray(expected));

    // the arrows are no answer
    for (const MenuKey key : {MenuKey::Up, MenuKey::Down, MenuKey::Left, MenuKey::Right})
    {
      EXPECT_THAT(menu.Press(key, {}, game), IsEmpty());
      EXPECT_EQ(menu.GetScreen(), MenuScreen::NewGame);
    }

    for (const MenuKey no : {MenuKey::No, MenuKey::Back})
    {
      EXPECT_THAT(menu.Press(no, {}, game), IsEmpty());
      EXPECT_EQ(menu.GetScreen(), MenuScreen::SinglePlayer);
      menu.Press(MenuKey::Select, {}, game);
    }

    EXPECT_THAT(menu.Press(MenuKey::Yes, {}, game), ElementsAre(new_game));
    EXPECT_FALSE(menu.IsOpen());

    Menu other = make_menu_in_single_player(0);
    EXPECT_THAT(other.Press(MenuKey::Select, {}, game), ElementsAre(new_game));
    EXPECT_FALSE(other.IsOpen());
  }

  TEST(MenuTest, GoesToSaveOnlyWhileAGameRunsThatIsNotBetweenLevels)
  {
    Menu menu = make_menu_in(0);
    menu.Press(MenuKey::Up);

    // the sound still comes, as in the original
    EXPECT_THAT(menu.Press(MenuKey::Select), ElementsAre(enter_sound));
    EXPECT_EQ(menu.GetScreen(), MenuScreen::SinglePlayer);

    MenuGame game = make_running_game();
    game.is_in_intermission = true;
    EXPECT_THAT(menu.Press(MenuKey::Select, {}, game), ElementsAre(enter_sound));
    EXPECT_EQ(menu.GetScreen(), MenuScreen::SinglePlayer);

    game.is_in_intermission = false;
    EXPECT_THAT(menu.Press(MenuKey::Select, {}, game), ElementsAre(enter_sound));
    EXPECT_EQ(menu.GetScreen(), MenuScreen::Save);
  }

  // ---- load and save

  TEST(MenuTest, LaysOutTheSlotsUsedAndUnused)
  {
    MenuGame game = make_running_game();

    // more than there is room for
    game.slots[11] = std::string(50, 'x');

    Menu menu = make_menu_in_single_player(1);
    ASSERT_EQ(menu.GetScreen(), MenuScreen::Load);
    menu.Press(MenuKey::Down, {}, game);

    std::vector<HudPicture> expected = {MakePicture("gfx/p_load.lmp", 108, 4, center)};
    append(expected, make_bronze_letters("the Slipgate Complex kills: 12/ 34", 16, 32));
    append(expected, make_bronze_letters("--- UNUSED SLOT ---", 16, 40));
    append(expected, make_bronze_letters("--- UNUSED SLOT ---", 16, 48));
    append(expected, make_bronze_letters("e1m2", 16, 56));
    for (int slot = 4; slot < 11; slot++)
    {
      append(expected, make_bronze_letters("--- UNUSED SLOT ---", 16, 32 + slot * 8));
    }
    append(expected, make_bronze_letters(std::string(38, 'x'), 16, 120));
    expected.push_back(MakeLetter(12, 8, 40, center));
    EXPECT_THAT(menu.Layout(0.0, {}, game), ElementsAreArray(expected));

    // the cursor blinks, four times a second
    EXPECT_EQ(menu.Layout(0.3, {}, game).back(), MakeLetter(13, 8, 40, center));
    EXPECT_EQ(menu.Layout(0.6, {}, game).back(), MakeLetter(12, 8, 40, center));
  }

  TEST(MenuTest, ShowsTheSameSlotsToSaveUnderAnotherTitle)
  {
    const MenuGame game = make_running_game();
    const Menu load = make_menu_in_single_player(1);
    const Menu save = make_menu_in_single_player(2);
    ASSERT_EQ(save.GetScreen(), MenuScreen::Save);

    std::vector<HudPicture> expected = load.Layout(0.0, {}, game);
    expected[0] = MakePicture("gfx/p_save.lmp", 108, 4, center);
    EXPECT_THAT(save.Layout(0.0, {}, game), ElementsAreArray(expected));
  }

  TEST(MenuTest, MovesAroundTheTwelveSlotsWithAllFourArrows)
  {
    Menu menu = make_menu_in_single_player(1);
    EXPECT_THAT(menu.Press(MenuKey::Up), ElementsAre(move_sound));
    EXPECT_EQ(menu.GetCursor(), 11);
    EXPECT_THAT(menu.Press(MenuKey::Right), ElementsAre(move_sound));
    EXPECT_EQ(menu.GetCursor(), 0);
    EXPECT_THAT(menu.Press(MenuKey::Left), ElementsAre(move_sound));
    EXPECT_EQ(menu.GetCursor(), 11);
    EXPECT_THAT(menu.Press(MenuKey::Down), ElementsAre(move_sound));
    EXPECT_THAT(menu.Press(MenuKey::Down), ElementsAre(move_sound));
    EXPECT_EQ(menu.GetCursor(), 1);

    EXPECT_THAT(menu.Press(MenuKey::Yes), IsEmpty());
    EXPECT_THAT(menu.Press(MenuKey::No), IsEmpty());
    EXPECT_EQ(menu.GetScreen(), MenuScreen::Load);
  }

  TEST(MenuTest, LoadsASlotThatIsUsedAndNotOneThatIsNot)
  {
    const MenuGame game = make_running_game();
    Menu menu = make_menu_in_single_player(1);

    menu.Press(MenuKey::Down, {}, game);
    EXPECT_THAT(menu.Press(MenuKey::Select, {}, game), ElementsAre(enter_sound));
    EXPECT_EQ(menu.GetScreen(), MenuScreen::Load);

    menu.Press(MenuKey::Down, {}, game);
    menu.Press(MenuKey::Down, {}, game);
    EXPECT_THAT(
      menu.Press(MenuKey::Select, {}, game),
      ElementsAre(enter_sound, MenuAction{.kind = MenuActionKind::LoadGame, .slot = 3}));
    EXPECT_FALSE(menu.IsOpen());
  }

  TEST(MenuTest, SavesIntoAnySlotAndSharesTheCursorWithLoad)
  {
    const MenuGame game = make_running_game();
    Menu menu = make_menu_in_single_player(1);
    menu.Press(MenuKey::Up, {}, game);
    menu.Press(MenuKey::Up, {}, game);
    EXPECT_THAT(menu.Press(MenuKey::Back, {}, game), ElementsAre(enter_sound));
    EXPECT_EQ(menu.GetScreen(), MenuScreen::SinglePlayer);

    menu.Press(MenuKey::Down, {}, game);
    menu.Press(MenuKey::Select, {}, game);
    ASSERT_EQ(menu.GetScreen(), MenuScreen::Save);
    EXPECT_EQ(menu.GetCursor(), 10);

    EXPECT_THAT(
      menu.Press(MenuKey::Select, {}, game),
      ElementsAre(MenuAction{.kind = MenuActionKind::SaveGame, .slot = 10}));
    EXPECT_FALSE(menu.IsOpen());
  }

  // ---- multiplayer

  TEST(MenuTest, ShowsMultiplayerWithTheLineThatNothingIsThere)
  {
    Menu menu = make_menu_in(1);
    EXPECT_THAT(menu.Press(MenuKey::Down), ElementsAre(move_sound));

    std::vector<HudPicture> expected = {
      MakePicture("gfx/qplaque.lmp", 16, 4, center),
      MakePicture("gfx/p_multi.lmp", 52, 4, center),
      MakePicture("gfx/mp_menu.lmp", 72, 32, center),
      MakePicture("gfx/menudot1.lmp", 54, 52, center),
    };
    append(expected, MakeLetters("No Communications Available", 52, 148, center));
    EXPECT_THAT(menu.Layout(0.0), ElementsAreArray(expected));

    // nothing on it leads anywhere
    EXPECT_THAT(menu.Press(MenuKey::Select), ElementsAre(enter_sound));
    EXPECT_EQ(menu.GetScreen(), MenuScreen::Multiplayer);

    menu.Press(MenuKey::Down);
    menu.Press(MenuKey::Down);
    EXPECT_EQ(menu.GetCursor(), 0);
    menu.Press(MenuKey::Up);
    EXPECT_EQ(menu.GetCursor(), 2);

    EXPECT_THAT(menu.Press(MenuKey::Back), ElementsAre(enter_sound));
    EXPECT_EQ(menu.GetScreen(), MenuScreen::Main);
  }

  // ---- options

  TEST(MenuTest, LaysOutTheOptionsWithSlidersAndWords)
  {
    MenuOptions options;
    options.screen_size = 30.0f;
    options.gamma = 0.75f;
    options.mouse_speed = 11.0f;
    options.music_volume = 0.0f;
    options.sound_volume = 1.0f;
    options.invert_mouse = true;

    Menu menu = make_menu_in(2);
    menu.Press(MenuKey::Down, options);

    const auto make_slider = [](const std::int32_t y, const std::int32_t knob)
    {
      std::vector<HudPicture> slider = {MakeLetter(128, 212, y, center)};
      for (int i = 0; i < 10; i++) { slider.push_back(MakeLetter(129, 220 + i * 8, y, center)); }
      slider.push_back(MakeLetter(130, 300, y, center));
      slider.push_back(MakeLetter(131, knob, y, center));
      return slider;
    };

    std::vector<HudPicture> expected = {
      MakePicture("gfx/qplaque.lmp", 16, 4, center),
      MakePicture("gfx/p_option.lmp", 88, 4, center),
    };
    append(expected, make_bronze_letters("Screen size", 104, 32));
    append(expected, make_slider(32, 220));
    append(expected, make_bronze_letters("Brightness", 112, 40));
    append(expected, make_slider(40, 256));
    append(expected, make_bronze_letters("Mouse Speed", 104, 48));
    append(expected, make_slider(48, 292));
    append(expected, make_bronze_letters("Music Volume", 96, 56));
    append(expected, make_slider(56, 220));
    append(expected, make_bronze_letters("Sound Volume", 96, 64));
    append(expected, make_slider(64, 292));
    append(expected, make_bronze_letters("Always Run", 112, 72));
    append(expected, make_bronze_letters("off", 220, 72));
    append(expected, make_bronze_letters("Invert Mouse", 96, 80));
    append(expected, make_bronze_letters("on", 220, 80));
    expected.push_back(MakeLetter(12, 200, 40, center));
    EXPECT_THAT(menu.Layout(0.0, options), ElementsAreArray(expected));

    EXPECT_EQ(menu.Layout(0.25, options).back(), MakeLetter(13, 200, 40, center));
  }

  TEST(MenuTest, PutsTheKnobOfASliderAtItsEndsAndItsMiddle)
  {
    const Menu menu = make_menu_in(2);
    const auto knob_of_size = [&](const float size)
    {
      MenuOptions options;
      options.screen_size = size;
      return find_knob(menu.Layout(0.0, options), 32);
    };

    EXPECT_EQ(knob_of_size(30.0f), 220);
    EXPECT_EQ(knob_of_size(75.0f), 256);
    EXPECT_EQ(knob_of_size(120.0f), 292);

    // what is beyond the ends, or no number, stays on the slider
    EXPECT_EQ(knob_of_size(-500.0f), 220);
    EXPECT_EQ(knob_of_size(1000.0f), 292);
    EXPECT_EQ(knob_of_size(std::numeric_limits<float>::quiet_NaN()), 220);

    // the brightness goes right as its number goes down
    MenuOptions options;
    options.gamma = 1.0f;
    EXPECT_EQ(find_knob(menu.Layout(0.0, options), 40), 220);
    options.gamma = 0.5f;
    EXPECT_EQ(find_knob(menu.Layout(0.0, options), 40), 292);
  }

  TEST(MenuTest, MovesAroundTheSevenOptions)
  {
    Menu menu = make_menu_in(2);
    EXPECT_THAT(menu.Press(MenuKey::Up), ElementsAre(move_sound));
    EXPECT_EQ(menu.GetCursor(), 6);
    EXPECT_THAT(menu.Press(MenuKey::Down), ElementsAre(move_sound));
    EXPECT_EQ(menu.GetCursor(), 0);

    EXPECT_THAT(menu.Press(MenuKey::Yes), IsEmpty());
    EXPECT_THAT(menu.Press(MenuKey::No), IsEmpty());

    EXPECT_THAT(menu.Press(MenuKey::Back), ElementsAre(enter_sound));
    EXPECT_EQ(menu.GetScreen(), MenuScreen::Main);
  }

  TEST(MenuTest, StepsEachSliderByItsStepAndStopsAtItsEnds)
  {
    struct Row
    {
      std::string_view name;
      float start;
      float after_right;
      float after_left;

      // the ends, and what a step beyond each gives
      float right_end;
      float left_end;
    };
    const Row rows[] = {
      {"viewsize", 100.0f, 110.0f, 90.0f, 120.0f, 30.0f},
      {"gamma", 0.8f, 0.75f, 0.85f, 0.5f, 1.0f},
      {"sensitivity", 3.0f, 3.5f, 2.5f, 11.0f, 1.0f},
      {"bgmvolume", 0.5f, 0.6f, 0.4f, 1.0f, 0.0f},
      {"volume", 0.7f, 0.8f, 0.6f, 1.0f, 0.0f},
    };

    Menu menu = make_menu_in(2);
    for (const Row &row : rows)
    {
      MenuOptions options;
      ASSERT_TRUE(options.Set(row.name, row.start));

      const auto press = [&](const MenuKey key)
      {
        const std::vector<MenuAction> actions = menu.Press(key, options);
        EXPECT_EQ(actions.size(), 2u);
        EXPECT_EQ(actions[0], change_sound);
        EXPECT_EQ(actions[1].kind, MenuActionKind::SetOption);
        EXPECT_EQ(actions[1].name, row.name);
        return actions[1].value;
      };

      EXPECT_FLOAT_EQ(press(MenuKey::Right), row.after_right) << row.name;
      EXPECT_FLOAT_EQ(press(MenuKey::Left), row.after_left) << row.name;

      // at an end it stays, and the sound still comes
      ASSERT_TRUE(options.Set(row.name, row.right_end));
      EXPECT_EQ(press(MenuKey::Right), row.right_end) << row.name;
      ASSERT_TRUE(options.Set(row.name, row.left_end));
      EXPECT_EQ(press(MenuKey::Left), row.left_end) << row.name;

      menu.Press(MenuKey::Down, options);
    }
  }

  TEST(MenuTest, ReachesBothEndsOfEverySliderInWholeSteps)
  {
    constexpr int steps[] = {9, 10, 20, 10, 10};

    Menu menu = make_menu_in(2);
    for (int row = 0; row < 5; row++)
    {
      MenuOptions options;
      const std::int32_t y = 32 + row * 8;

      // all the way left, then right, as a host does it
      for (int i = 0; i < 25; i++)
      {
        for (const MenuAction &action : menu.Press(MenuKey::Left, options)) { options.Set(action.name, action.value); }
      }
      EXPECT_EQ(find_knob(menu.Layout(0.0, options), y), 220) << row;

      for (int i = 0; i < steps[row]; i++)
      {
        EXPECT_NE(find_knob(menu.Layout(0.0, options), y), 292) << row << " after " << i;
        for (const MenuAction &action : menu.Press(MenuKey::Right, options)) { options.Set(action.name, action.value); }
      }
      EXPECT_EQ(find_knob(menu.Layout(0.0, options), y), 292) << row;

      menu.Press(MenuKey::Down, options);
    }
  }

  TEST(MenuTest, TurnsAnOptionOverWithEitherArrowAndWithSelect)
  {
    Menu menu = make_menu_in(2);
    for (int i = 0; i < 5; i++) { menu.Press(MenuKey::Down); }

    MenuOptions options;
    EXPECT_THAT(menu.Press(MenuKey::Left, options), ElementsAre(change_sound, set_option("always_run", 1.0f)));
    EXPECT_THAT(menu.Press(MenuKey::Right, options), ElementsAre(change_sound, set_option("always_run", 1.0f)));
    options.always_run = true;
    EXPECT_THAT(menu.Press(MenuKey::Right, options), ElementsAre(change_sound, set_option("always_run", 0.0f)));

    // enter changes, and gives its own sound as well, as in the original
    menu.Press(MenuKey::Down);
    EXPECT_THAT(
      menu.Press(MenuKey::Select, options),
      ElementsAre(change_sound, set_option("invert_mouse", 1.0f), enter_sound));
    EXPECT_EQ(menu.GetScreen(), MenuScreen::Options);

    // and is a step to the right on a slider
    menu.Press(MenuKey::Down);
    EXPECT_THAT(
      menu.Press(MenuKey::Select, options),
      ElementsAre(change_sound, set_option("viewsize", 120.0f), enter_sound));
  }

  TEST(MenuOptionsTest, SetsAnOptionByItsName)
  {
    MenuOptions options;
    EXPECT_TRUE(options.Set("viewsize", 60.0f));
    EXPECT_TRUE(options.Set("gamma", 0.6f));
    EXPECT_TRUE(options.Set("sensitivity", 7.0f));
    EXPECT_TRUE(options.Set("bgmvolume", 0.2f));
    EXPECT_TRUE(options.Set("volume", 0.3f));
    EXPECT_TRUE(options.Set("always_run", 1.0f));
    EXPECT_TRUE(options.Set("invert_mouse", 1.0f));
    EXPECT_FALSE(options.Set("lookspring", 1.0f));

    MenuOptions expected;
    expected.screen_size = 60.0f;
    expected.gamma = 0.6f;
    expected.mouse_speed = 7.0f;
    expected.music_volume = 0.2f;
    expected.sound_volume = 0.3f;
    expected.always_run = true;
    expected.invert_mouse = true;
    EXPECT_EQ(options, expected);
  }

  // ---- help

  TEST(MenuTest, TurnsTheSixPagesOfHelpBothWaysAround)
  {
    Menu menu = make_menu_in(3);
    EXPECT_THAT(menu.Layout(5.0), ElementsAre(MakePicture("gfx/help0.lmp", 0, 0, center)));

    EXPECT_THAT(menu.Press(MenuKey::Right), ElementsAre(enter_sound));
    EXPECT_THAT(menu.Layout(5.0), ElementsAre(MakePicture("gfx/help1.lmp", 0, 0, center)));
    EXPECT_THAT(menu.Press(MenuKey::Up), ElementsAre(enter_sound));
    EXPECT_EQ(menu.GetCursor(), 2);
    EXPECT_THAT(menu.Press(MenuKey::Left), ElementsAre(enter_sound));
    EXPECT_THAT(menu.Press(MenuKey::Down), ElementsAre(enter_sound));
    EXPECT_THAT(menu.Press(MenuKey::Down), ElementsAre(enter_sound));
    EXPECT_THAT(menu.Layout(5.0), ElementsAre(MakePicture("gfx/help5.lmp", 0, 0, center)));
    EXPECT_THAT(menu.Press(MenuKey::Right), ElementsAre(enter_sound));
    EXPECT_EQ(menu.GetCursor(), 0);

    // enter and the letters do nothing
    menu.Press(MenuKey::Right);
    for (const MenuKey key : {MenuKey::Select, MenuKey::Yes, MenuKey::No}) { EXPECT_THAT(menu.Press(key), IsEmpty()); }
    EXPECT_EQ(menu.GetScreen(), MenuScreen::Help);

    // it starts at the first page each time
    EXPECT_THAT(menu.Press(MenuKey::Back), ElementsAre(enter_sound));
    EXPECT_EQ(menu.GetScreen(), MenuScreen::Main);
    menu.Press(MenuKey::Select);
    EXPECT_THAT(menu.Layout(5.0), ElementsAre(MakePicture("gfx/help0.lmp", 0, 0, center)));
  }

  // ---- quit

  TEST(MenuTest, AsksInABoxOverTheMainMenuBeforeItQuits)
  {
    Menu menu = make_menu_in(4);
    ASSERT_EQ(menu.GetScreen(), MenuScreen::Quit);

    std::vector<HudPicture> expected = {
      MakePicture("gfx/qplaque.lmp", 16, 4, center),
      MakePicture("gfx/ttl_main.lmp", 112, 4, center),
      MakePicture("gfx/mainmenu.lmp", 72, 32, center),
      MakePicture("gfx/menudot1.lmp", 54, 112, center),

      // the box: its left side, twelve pieces of the middle, its right
      MakePicture("gfx/box_tl.lmp", 56, 76, center),
      MakePicture("gfx/box_ml.lmp", 56, 84, center),
      MakePicture("gfx/box_ml.lmp", 56, 92, center),
      MakePicture("gfx/box_ml.lmp", 56, 100, center),
      MakePicture("gfx/box_ml.lmp", 56, 108, center),
      MakePicture("gfx/box_bl.lmp", 56, 116, center),
    };
    for (int piece = 0; piece < 12; piece++)
    {
      const std::int32_t x = 64 + piece * 16;
      expected.push_back(MakePicture("gfx/box_tm.lmp", x, 76, center));
      expected.push_back(MakePicture("gfx/box_mm.lmp", x, 84, center));
      expected.push_back(MakePicture("gfx/box_mm2.lmp", x, 92, center));
      expected.push_back(MakePicture("gfx/box_mm2.lmp", x, 100, center));
      expected.push_back(MakePicture("gfx/box_mm2.lmp", x, 108, center));
      expected.push_back(MakePicture("gfx/box_bm.lmp", x, 116, center));
    }
    expected.push_back(MakePicture("gfx/box_tr.lmp", 256, 76, center));
    expected.push_back(MakePicture("gfx/box_mr.lmp", 256, 84, center));
    expected.push_back(MakePicture("gfx/box_mr.lmp", 256, 92, center));
    expected.push_back(MakePicture("gfx/box_mr.lmp", 256, 100, center));
    expected.push_back(MakePicture("gfx/box_mr.lmp", 256, 108, center));
    expected.push_back(MakePicture("gfx/box_br.lmp", 256, 116, center));

    append(expected, make_bronze_letters("Do you really want to", 72, 84));
    append(expected, make_bronze_letters("leave the game?", 96, 92));
    append(expected, make_bronze_letters("Y leaves, N stays.", 88, 108));
    EXPECT_THAT(menu.Layout(0.0), ElementsAreArray(expected));
  }

  TEST(MenuTest, QuitsOnYesAndGoesBackOnNo)
  {
    for (const MenuKey no : {MenuKey::No, MenuKey::Back})
    {
      Menu menu = make_menu_in(4);
      EXPECT_THAT(menu.Press(no), ElementsAre(enter_sound));
      EXPECT_EQ(menu.GetScreen(), MenuScreen::Main);
      EXPECT_EQ(menu.GetCursor(), 4);
    }

    for (const MenuKey yes : {MenuKey::Yes, MenuKey::Select})
    {
      Menu menu = make_menu_in(4);
      EXPECT_THAT(menu.Press(yes), ElementsAre(quit));
    }

    Menu menu = make_menu_in(4);
    for (const MenuKey key : {MenuKey::Up, MenuKey::Down, MenuKey::Left, MenuKey::Right})
    {
      EXPECT_THAT(menu.Press(key), IsEmpty());
      EXPECT_EQ(menu.GetScreen(), MenuScreen::Quit);
    }
  }

  // ---- all the way out

  TEST(MenuTest, GoesBackScreenByScreenUntilItIsClosed)
  {
    const MenuGame game = make_running_game();

    // from the deepest screens: the slots, and the question
    for (const int item : {0, 1, 2})
    {
      Menu menu = make_menu_in_single_player(item);
      ASSERT_NE(menu.GetScreen(), MenuScreen::SinglePlayer) << item;

      menu.Press(MenuKey::Back, {}, game);
      EXPECT_EQ(menu.GetScreen(), MenuScreen::SinglePlayer) << item;
      menu.Press(MenuKey::Back, {}, game);
      EXPECT_EQ(menu.GetScreen(), MenuScreen::Main) << item;
      EXPECT_THAT(menu.Press(MenuKey::Back, {}, game), ElementsAre(resume)) << item;
      EXPECT_FALSE(menu.IsOpen()) << item;
    }

    // from each screen of the main menu
    for (int item = 0; item < 5; item++)
    {
      Menu menu = make_menu_in(item);
      menu.Press(MenuKey::Back);
      EXPECT_EQ(menu.GetScreen(), MenuScreen::Main) << item;
      EXPECT_THAT(menu.Press(MenuKey::Back), ElementsAre(resume)) << item;
      EXPECT_FALSE(menu.IsOpen()) << item;
    }
  }

  // ---- the box and the pause

  TEST(MenuTest, BuildsABoxOfAnySize)
  {
    std::vector<HudPicture> pictures;
    Menu::AddTextBox(pictures, 8, 16, 3, 1);
    EXPECT_THAT(pictures, ElementsAre(
      MakePicture("gfx/box_tl.lmp", 8, 16, center),
      MakePicture("gfx/box_ml.lmp", 8, 24, center),
      MakePicture("gfx/box_bl.lmp", 8, 32, center),

      // three letters wide takes two pieces of two
      MakePicture("gfx/box_tm.lmp", 16, 16, center),
      MakePicture("gfx/box_mm.lmp", 16, 24, center),
      MakePicture("gfx/box_bm.lmp", 16, 32, center),
      MakePicture("gfx/box_tm.lmp", 32, 16, center),
      MakePicture("gfx/box_mm.lmp", 32, 24, center),
      MakePicture("gfx/box_bm.lmp", 32, 32, center),

      MakePicture("gfx/box_tr.lmp", 48, 16, center),
      MakePicture("gfx/box_mr.lmp", 48, 24, center),
      MakePicture("gfx/box_br.lmp", 48, 32, center)));

    // a box for nothing is its corners alone
    pictures.clear();
    Menu::AddTextBox(pictures, 0, 0, 0, -4);
    EXPECT_THAT(pictures, ElementsAre(
      MakePicture("gfx/box_tl.lmp", 0, 0, center),
      MakePicture("gfx/box_bl.lmp", 0, 8, center),
      MakePicture("gfx/box_tr.lmp", 8, 0, center),
      MakePicture("gfx/box_br.lmp", 8, 8, center)));
  }

  TEST(MenuTest, PutsThePausePictureAboveTheMiddle)
  {
    EXPECT_THAT(Menu::LayoutPause(), ElementsAre(MakePicture("gfx/pause.lmp", 96, 64, center)));
    EXPECT_THAT(Menu::LayoutPause(86, 28), ElementsAre(MakePicture("gfx/pause.lmp", 117, 62, center)));
  }
}
