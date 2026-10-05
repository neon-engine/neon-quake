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
  using quake::PromptDevice;
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

  // The picture of the items holds multiplayer too, which is not there yet:
  // its first item is drawn, and what comes after the second in its place.
  TEST(MenuTest, LaysOutTheMainMenuWithoutMultiplayerAndTheCursorOnEachItem)
  {
    Menu menu;
    menu.Open();
    for (int item = 0; item < 4; item++)
    {
      EXPECT_EQ(menu.GetCursor(), item);
      EXPECT_THAT(menu.Layout(0.0), ElementsAre(
        MakePicture("gfx/qplaque.lmp", 16, 4, center),
        MakePicture("gfx/ttl_main.lmp", 112, 4, center),
        MakeStrip("gfx/mainmenu.lmp", 72, 32, 0, 20, center),
        MakeStrip("gfx/mainmenu.lmp", 72, 52, 40, 0, center),
        MakePicture("gfx/menudot1.lmp", 54, 32 + item * 20, center))) << item;

      EXPECT_THAT(menu.Press(MenuKey::Down), ElementsAre(move_sound));
    }

    // around, to the first
    EXPECT_EQ(menu.GetCursor(), 0);
    EXPECT_THAT(menu.Press(MenuKey::Up), ElementsAre(move_sound));
    EXPECT_EQ(menu.GetCursor(), 3);
    EXPECT_THAT(menu.Press(MenuKey::Up), ElementsAre(move_sound));
    EXPECT_EQ(menu.GetCursor(), 2);
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
    widths.load = 72;
    widths.save = 74;
    widths.options = 114;

    EXPECT_EQ(make_menu_at(0).Layout(0.0, {}, {}, widths)[1], MakePicture("gfx/ttl_main.lmp", 82, 4, center));
    EXPECT_EQ(make_menu_in(0).Layout(0.0, {}, {}, widths)[1], MakePicture("gfx/ttl_sgl.lmp", 125, 4, center));
    EXPECT_EQ(make_menu_in(1).Layout(0.0, {}, {}, widths)[1], MakePicture("gfx/p_option.lmp", 103, 4, center));
    EXPECT_EQ(
      make_menu_in_single_player(1).Layout(0.0, {}, {}, widths)[0], MakePicture("gfx/p_load.lmp", 124, 4, center));
    EXPECT_EQ(
      make_menu_in_single_player(2).Layout(0.0, {}, {}, widths)[0], MakePicture("gfx/p_save.lmp", 123, 4, center));
  }

  TEST(MenuTest, GoesFromTheMainMenuIntoEachScreen)
  {
    constexpr MenuScreen screens[] = {
      MenuScreen::SinglePlayer, MenuScreen::Options, MenuScreen::Help, MenuScreen::Quit,
    };
    for (int item = 0; item < 4; item++)
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

    Menu menu = make_menu_in(1);
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
    append(expected, make_bronze_letters("Stick Speed", 104, 88));
    append(expected, make_slider(88, 234));
    append(expected, make_bronze_letters("Video Options", 88, 96));
    expected.push_back(MakeLetter(12, 200, 40, center));
    EXPECT_THAT(menu.Layout(0.0, options), ElementsAreArray(expected));

    EXPECT_EQ(menu.Layout(0.25, options).back(), MakeLetter(13, 200, 40, center));
  }

  TEST(MenuTest, PutsTheKnobOfASliderAtItsEndsAndItsMiddle)
  {
    const Menu menu = make_menu_in(1);
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

  TEST(MenuTest, MovesAroundTheEightOptionsAndTheItemOfTheVideoSettings)
  {
    Menu menu = make_menu_in(1);
    EXPECT_THAT(menu.Press(MenuKey::Up), ElementsAre(move_sound));
    EXPECT_EQ(menu.GetCursor(), 8);
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

    Menu menu = make_menu_in(1);
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

    Menu menu = make_menu_in(1);
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
    Menu menu = make_menu_in(1);
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
      ElementsAre(change_sound, set_option("joy_sensitivity", 3.5f), enter_sound));
  }

  // ---- video

  /// A menu on the screen of the video settings.
  Menu make_menu_in_video()
  {
    Menu menu = make_menu_in(1);
    menu.Press(MenuKey::Up);
    menu.Press(MenuKey::Select);
    return menu;
  }

  /// A display that offers three sizes, the largest first.
  MenuGame make_game_with_display()
  {
    MenuGame game;
    game.display_sizes = {{3840, 2160}, {1920, 1080}, {1280, 720}};
    return game;
  }

  TEST(MenuTest, TheLastItemOfTheOptionsLeadsToTheVideoSettingsAndBackLeadsToTheOptions)
  {
    Menu menu = make_menu_in(1);
    menu.Press(MenuKey::Up);

    // it has no value to change
    EXPECT_THAT(menu.Press(MenuKey::Left), IsEmpty());
    EXPECT_THAT(menu.Press(MenuKey::Right), IsEmpty());

    EXPECT_THAT(menu.Press(MenuKey::Select), ElementsAre(enter_sound));
    EXPECT_EQ(menu.GetScreen(), MenuScreen::Video);
    EXPECT_EQ(menu.GetCursor(), 0);

    EXPECT_THAT(menu.Press(MenuKey::Back), ElementsAre(enter_sound));
    EXPECT_EQ(menu.GetScreen(), MenuScreen::Options);
    EXPECT_EQ(menu.GetCursor(), 8) << "where it was left";
  }

  TEST(MenuTest, MovesAroundTheFiveVideoSettings)
  {
    Menu menu = make_menu_in_video();
    EXPECT_THAT(menu.Press(MenuKey::Up), ElementsAre(move_sound));
    EXPECT_EQ(menu.GetCursor(), 4);
    EXPECT_THAT(menu.Press(MenuKey::Down), ElementsAre(move_sound));
    EXPECT_EQ(menu.GetCursor(), 0);
  }

  TEST(MenuTest, TheCursorPassesOverTheSizeWhileTheWindowHasNoBorders)
  {
    Menu menu = make_menu_in_video();
    MenuOptions options;
    options.window_mode = 1.0f;

    // from the mode down to vertical sync, and back up to the mode
    EXPECT_THAT(menu.Press(MenuKey::Down, options), ElementsAre(move_sound));
    EXPECT_EQ(menu.GetCursor(), 2);
    EXPECT_THAT(menu.Press(MenuKey::Up, options), ElementsAre(move_sound));
    EXPECT_EQ(menu.GetCursor(), 0);

    // as a window, or over a display that is switched to it, the size counts
    for (const float mode : {0.0f, 2.0f})
    {
      options.window_mode = mode;
      menu.Press(MenuKey::Down, options);
      EXPECT_EQ(menu.GetCursor(), 1) << mode;
      menu.Press(MenuKey::Up, options);
    }
  }

  TEST(MenuTest, TheCursorPassesOverTheFrameLimitWhileThereIsNone)
  {
    Menu menu = make_menu_in_video();
    MenuOptions options;
    options.unlimited_frames = true;

    // up from the mode is the setting for no limit, and up from there
    // vertical sync
    menu.Press(MenuKey::Up, options);
    EXPECT_EQ(menu.GetCursor(), 4);
    menu.Press(MenuKey::Up, options);
    EXPECT_EQ(menu.GetCursor(), 2);
    menu.Press(MenuKey::Down, options);
    EXPECT_EQ(menu.GetCursor(), 4);
  }

  TEST(MenuTest, ASettingThatIsGreyedOutStaysAsItIs)
  {
    // the cursor was left on the size, and then the window lost its borders
    Menu menu = make_menu_in_video();
    menu.Press(MenuKey::Down);
    MenuOptions options;
    options.window_mode = 1.0f;
    options.window_width = 1920.0f;
    options.window_height = 1080.0f;

    for (const MenuKey key : {MenuKey::Left, MenuKey::Right})
    {
      EXPECT_THAT(menu.Press(key, options, make_game_with_display()), ElementsAre(change_sound));
    }
  }

  TEST(MenuTest, GoesThroughTheModesOfTheWindowAndAround)
  {
    Menu menu = make_menu_in_video();
    MenuOptions options;
    options.window_mode = 0.0f;

    EXPECT_THAT(menu.Press(MenuKey::Right, options), ElementsAre(change_sound, set_option("vid_mode", 1.0f)));
    options.window_mode = 2.0f;
    EXPECT_THAT(menu.Press(MenuKey::Right, options), ElementsAre(change_sound, set_option("vid_mode", 0.0f)));
    options.window_mode = 0.0f;
    EXPECT_THAT(menu.Press(MenuKey::Left, options), ElementsAre(change_sound, set_option("vid_mode", 2.0f)));

    // enter is a step to the right, with its own sound after
    EXPECT_THAT(
      menu.Press(MenuKey::Select, options), ElementsAre(change_sound, set_option("vid_mode", 1.0f), enter_sound));
  }

  TEST(MenuTest, GoesThroughTheSizesTheDisplayOffersAndStopsAtItsEnds)
  {
    Menu menu = make_menu_in_video();
    menu.Press(MenuKey::Down);
    const MenuGame game = make_game_with_display();
    MenuOptions options;
    options.window_width = 1920.0f;
    options.window_height = 1080.0f;

    // to the right is larger
    EXPECT_THAT(
      menu.Press(MenuKey::Right, options, game),
      ElementsAre(change_sound, set_option("vid_width", 3840.0f), set_option("vid_height", 2160.0f)));
    EXPECT_THAT(
      menu.Press(MenuKey::Left, options, game),
      ElementsAre(change_sound, set_option("vid_width", 1280.0f), set_option("vid_height", 720.0f)));

    // at the largest it stays
    options.window_width = 3840.0f;
    options.window_height = 2160.0f;
    EXPECT_THAT(
      menu.Press(MenuKey::Right, options, game),
      ElementsAre(change_sound, set_option("vid_width", 3840.0f), set_option("vid_height", 2160.0f)));

    // a size the display does not offer goes to the largest
    options.window_width = 1000.0f;
    options.window_height = 700.0f;
    EXPECT_THAT(
      menu.Press(MenuKey::Left, options, game),
      ElementsAre(change_sound, set_option("vid_width", 3840.0f), set_option("vid_height", 2160.0f)));

    // and without a display there is nothing to choose
    EXPECT_THAT(menu.Press(MenuKey::Right, options), ElementsAre(change_sound));
  }

  TEST(MenuTest, TurnsVerticalSyncOver)
  {
    Menu menu = make_menu_in_video();
    menu.Press(MenuKey::Down);
    menu.Press(MenuKey::Down);
    MenuOptions options;

    EXPECT_THAT(menu.Press(MenuKey::Right, options), ElementsAre(change_sound, set_option("vid_vsync", 1.0f)));
    options.vertical_sync = true;
    EXPECT_THAT(menu.Press(MenuKey::Left, options), ElementsAre(change_sound, set_option("vid_vsync", 0.0f)));
  }

  TEST(MenuTest, SlidesTheFrameLimitInTensFromThirtyToThreeHundred)
  {
    Menu menu = make_menu_in_video();
    for (int i = 0; i < 3; i++) { menu.Press(MenuKey::Down); }
    ASSERT_EQ(menu.GetCursor(), Menu::frame_limit_item);
    MenuOptions options;

    options.frame_limit = 60.0f;
    EXPECT_THAT(menu.Press(MenuKey::Right, options), ElementsAre(change_sound, set_option("vid_maxfps", 70.0f)));
    EXPECT_THAT(menu.Press(MenuKey::Left, options), ElementsAre(change_sound, set_option("vid_maxfps", 50.0f)));

    // it stops at the least and at the most
    options.frame_limit = 30.0f;
    EXPECT_THAT(menu.Press(MenuKey::Left, options), ElementsAre(change_sound, set_option("vid_maxfps", 30.0f)));
    options.frame_limit = 300.0f;
    EXPECT_THAT(menu.Press(MenuKey::Right, options), ElementsAre(change_sound, set_option("vid_maxfps", 300.0f)));

    // a number between two places goes on from the nearest
    options.frame_limit = 144.0f;
    EXPECT_THAT(menu.Press(MenuKey::Right, options), ElementsAre(change_sound, set_option("vid_maxfps", 150.0f)));
  }

  TEST(MenuTest, TurnsTheLimitOfTheFramesOffAndOn)
  {
    Menu menu = make_menu_in_video();
    menu.Press(MenuKey::Up);
    MenuOptions options;
    options.frame_limit = 90.0f;

    // the number is not touched: it is there again when the limit is
    EXPECT_THAT(menu.Press(MenuKey::Right, options), ElementsAre(change_sound, set_option("vid_unlimited", 1.0f)));
    options.unlimited_frames = true;
    EXPECT_THAT(menu.Press(MenuKey::Left, options), ElementsAre(change_sound, set_option("vid_unlimited", 0.0f)));
    EXPECT_THAT(
      menu.Press(MenuKey::Select, options), ElementsAre(change_sound, set_option("vid_unlimited", 0.0f), enter_sound));
  }

  TEST(MenuTest, LaysOutTheVideoSettingsWithTheirWords)
  {
    Menu menu = make_menu_in_video();
    MenuOptions options;
    options.window_mode = 2.0f;
    options.window_width = 1920.0f;
    options.window_height = 1080.0f;
    options.vertical_sync = true;

    std::vector<HudPicture> expected = {
      MakePicture("gfx/qplaque.lmp", 16, 4, center),
      MakePicture("gfx/vidmodes.lmp", 52, 4, center),
    };
    append(expected, make_bronze_letters("Video Mode", 112, 32));
    append(expected, make_bronze_letters("fullscreen", 220, 32));
    append(expected, make_bronze_letters("Resolution", 112, 40));
    append(expected, make_bronze_letters("1920x1080", 220, 40));
    append(expected, make_bronze_letters("Vertical Sync", 88, 48));
    append(expected, make_bronze_letters("on", 220, 48));

    // the limit with its name, and a slider of ten letters between its
    // ends with the knob a ninth of the way along 72 pixels: 60 is the
    // fourth of the 28 places from 30 to 300
    options.frame_limit = 60.0f;
    append(expected, make_bronze_letters("Max FPS 60", 112, 56));
    expected.push_back(MakeLetter(128, 212, 56, center));
    for (std::int32_t i = 0; i < 10; i++) { expected.push_back(MakeLetter(129, 220 + i * 8, 56, center)); }
    expected.push_back(MakeLetter(130, 300, 56, center));
    expected.push_back(MakeLetter(131, 220 + 72 * 3 / 27, 56, center));

    append(expected, make_bronze_letters("Unlimited FPS", 88, 64));
    append(expected, make_bronze_letters("off", 220, 64));

    expected.push_back(MakeLetter(12, 200, 32, center));
    EXPECT_THAT(menu.Layout(0.0, options), ElementsAreArray(expected));

    // a size that was never chosen says so
    options.window_width = 0.0f;
    const std::vector<HudPicture> unset = menu.Layout(0.0, options);
    const std::vector<HudPicture> words = make_bronze_letters("as started", 220, 40);
    EXPECT_TRUE(std::ranges::search(unset, words).begin() != unset.end());
  }

  TEST(MenuTest, GreysOutTheSizeWithoutBordersAndTheLimitWhenThereIsNone)
  {
    Menu menu = make_menu_in_video();
    MenuOptions options;
    options.window_mode = 1.0f;
    options.window_width = 1920.0f;
    options.window_height = 1080.0f;
    options.frame_limit = 60.0f;
    options.unlimited_frames = true;

    // in the plain letters, where the others are in the coloured ones, and
    // the slider, which has no plain letters, is not there
    std::vector<HudPicture> expected = {
      MakePicture("gfx/qplaque.lmp", 16, 4, center),
      MakePicture("gfx/vidmodes.lmp", 52, 4, center),
    };
    append(expected, make_bronze_letters("Video Mode", 112, 32));
    append(expected, make_bronze_letters("borderless", 220, 32));
    append(expected, MakeLetters("Resolution", 112, 40, center));
    append(expected, MakeLetters("1920x1080", 220, 40, center));
    append(expected, make_bronze_letters("Vertical Sync", 88, 48));
    append(expected, make_bronze_letters("off", 220, 48));
    append(expected, MakeLetters("Max FPS 60", 112, 56, center));
    append(expected, make_bronze_letters("Unlimited FPS", 88, 64));
    append(expected, make_bronze_letters("on", 220, 64));
    expected.push_back(MakeLetter(12, 200, 32, center));
    EXPECT_THAT(menu.Layout(0.0, options), ElementsAreArray(expected));
  }

  TEST(MenuOptionsTest, KeepsTheNumberOfTheLimitWhileThereIsNone)
  {
    MenuOptions options;
    EXPECT_FALSE(options.HasFrameLimit()) << "as the game was started, until the host says what that is";

    EXPECT_TRUE(options.Set("vid_maxfps", 90.0f));
    EXPECT_TRUE(options.HasFrameLimit());
    EXPECT_EQ(options.GetFrameLimit(), 90);

    EXPECT_TRUE(options.Set("vid_unlimited", 1.0f));
    EXPECT_EQ(options.GetFrameLimit(), 0) << "none";
    EXPECT_EQ(options.frame_limit, 90.0f);

    EXPECT_TRUE(options.Set("vid_unlimited", 0.0f));
    EXPECT_EQ(options.GetFrameLimit(), 90);

    // held to what a limit can be
    options.frame_limit = 12.0f;
    EXPECT_EQ(options.GetFrameLimit(), 30);
    options.frame_limit = 1000.0f;
    EXPECT_EQ(options.GetFrameLimit(), 300);
  }

  TEST(MenuOptionsTest, ReadsALimitOfNoneOfAnOlderFileAsNoLimit)
  {
    // 0 was no limit before there was a setting of its own for it
    MenuOptions options;
    EXPECT_TRUE(options.Set("vid_maxfps", 0.0f));
    EXPECT_TRUE(options.unlimited_frames);
    EXPECT_EQ(options.GetFrameLimit(), 0);
    EXPECT_LT(options.frame_limit, 0.0f) << "the number is still to be chosen";
  }

  // ---- the hints

  TEST(MenuTest, ShowsTheButtonsThatChooseAndGoBackBelowAMenu)
  {
    // a picture of 16 pixels, its word 4 pixels after it and in the middle
    // of its height, and 16 pixels to the next: 136 pixels in the middle
    // of 320
    const Menu menu = make_menu_at(0);
    std::vector<HudPicture> expected = {MakePicture("prompts/key-enter.png", 92, 152, center)};
    append(expected, make_bronze_letters("Select", 112, 156));
    expected.push_back(MakePicture("prompts/key-escape.png", 176, 152, center));
    append(expected, make_bronze_letters("Back", 196, 156));
    EXPECT_THAT(menu.LayoutHints(PromptDevice::Keyboard), ElementsAreArray(expected));
  }

  TEST(MenuTest, ShowsTheButtonsOfWhatThePlayerHolds)
  {
    const Menu menu = make_menu_at(0);
    using Names = std::pair<std::string_view, std::string_view>;
    const auto names_of = [&](const PromptDevice device)
    {
      const std::vector<HudPicture> hints = menu.LayoutHints(device);
      return Names(hints.front().name, hints[7].name);
    };

    EXPECT_EQ(names_of(PromptDevice::Xbox), Names("prompts/xbox-a.png", "prompts/xbox-b.png"));
    EXPECT_EQ(
      names_of(PromptDevice::PlayStation), Names("prompts/playstation-cross.png", "prompts/playstation-circle.png"));

    // the menus are bound by where a button is: on a Switch controller the
    // one below is B and the one to the right is A
    EXPECT_EQ(names_of(PromptDevice::Switch), Names("prompts/switch-b.png", "prompts/switch-a.png"));
  }

  TEST(MenuTest, ShowsNoHintsWhereAScreenSaysItsKeysItselfOrTheMenusAreClosed)
  {
    Menu closed;
    EXPECT_THAT(closed.LayoutHints(PromptDevice::Xbox), IsEmpty());

    // help, and the question before the game is left
    EXPECT_THAT(make_menu_in(2).LayoutHints(PromptDevice::Xbox), IsEmpty());
    EXPECT_THAT(make_menu_in(3).LayoutHints(PromptDevice::Xbox), IsEmpty());

    // the options and the video settings have them
    EXPECT_FALSE(make_menu_in(1).LayoutHints(PromptDevice::Xbox).empty());
    EXPECT_FALSE(make_menu_in_video().LayoutHints(PromptDevice::Xbox).empty());
  }

  TEST(MenuOptionsTest, SetsTheVideoSettingsByTheirNames)
  {
    MenuOptions options;
    EXPECT_LT(options.window_mode, 0.0f) << "as the game was started, until the host says which that is";
    EXPECT_FALSE(options.vertical_sync);

    EXPECT_TRUE(options.Set("vid_mode", 2.0f));
    EXPECT_TRUE(options.Set("vid_width", 1280.0f));
    EXPECT_TRUE(options.Set("vid_height", 720.0f));
    EXPECT_TRUE(options.Set("vid_vsync", 1.0f));
    EXPECT_TRUE(options.Set("vid_maxfps", 144.0f));
    EXPECT_EQ(options.frame_limit, 144.0f);

    EXPECT_EQ(options.window_mode, 2.0f);
    EXPECT_EQ(options.window_width, 1280.0f);
    EXPECT_EQ(options.window_height, 720.0f);
    EXPECT_TRUE(options.vertical_sync);
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
    Menu menu = make_menu_in(2);
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
    Menu menu = make_menu_in(3);
    ASSERT_EQ(menu.GetScreen(), MenuScreen::Quit);

    std::vector<HudPicture> expected = {
      MakePicture("gfx/qplaque.lmp", 16, 4, center),
      MakePicture("gfx/ttl_main.lmp", 112, 4, center),
      MakeStrip("gfx/mainmenu.lmp", 72, 32, 0, 20, center),
      MakeStrip("gfx/mainmenu.lmp", 72, 52, 40, 0, center),
      MakePicture("gfx/menudot1.lmp", 54, 92, center),

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
      Menu menu = make_menu_in(3);
      EXPECT_THAT(menu.Press(no), ElementsAre(enter_sound));
      EXPECT_EQ(menu.GetScreen(), MenuScreen::Main);
      EXPECT_EQ(menu.GetCursor(), 3);
    }

    for (const MenuKey yes : {MenuKey::Yes, MenuKey::Select})
    {
      Menu menu = make_menu_in(3);
      EXPECT_THAT(menu.Press(yes), ElementsAre(quit));
    }

    Menu menu = make_menu_in(3);
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
    for (int item = 0; item < 4; item++)
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
