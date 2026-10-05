#include "menu.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

#include "center-text.hpp"
#include "hud-text.hpp"

namespace quake
{
  // The places and numbers of the original's screens, the rows of the
  // options, and small helpers of the layouts.
  namespace
  {
    constexpr HudAnchor anchor = HudAnchor::Center;

    constexpr std::int32_t screen_width = 320;

    /// Where the titles and the first row of every screen are.
    constexpr std::int32_t title_y = 4;
    constexpr std::int32_t first_row_y = 32;

    /// How far apart the items of a picture of items are, and the lines
    /// of a screen of text.
    constexpr std::int32_t item_height = 20;
    constexpr std::int32_t line_height = 8;

    /// The letter that is the cursor of a screen of text, and the one it
    /// changes with as it blinks.
    constexpr std::int32_t text_cursor = 12;

    /// The letters a slider is made of: its left end, its middle, of
    /// which it has ten, its right end, and the knob.
    constexpr std::int32_t slider_left = 128;
    constexpr std::int32_t slider_middle = 129;
    constexpr std::int32_t slider_right = 130;
    constexpr std::int32_t slider_knob = 131;
    constexpr std::int32_t slider_length = 10;

    /// Where the words of the options end, where their cursor is, and
    /// where the sliders and the words on and off start.
    constexpr std::int32_t option_label_end = 192;
    constexpr std::int32_t option_cursor_x = 200;
    constexpr std::int32_t option_value_x = 220;

    /// The item of the settings that leads to the video settings.
    constexpr std::string_view video_label = "Video Options";

    constexpr std::array<std::string_view, 6> cursor_names = {
      "gfx/menudot1.lmp", "gfx/menudot2.lmp", "gfx/menudot3.lmp",
      "gfx/menudot4.lmp", "gfx/menudot5.lmp", "gfx/menudot6.lmp",
    };

    constexpr std::array<std::string_view, Menu::help_pages> help_names = {
      "gfx/help0.lmp", "gfx/help1.lmp", "gfx/help2.lmp", "gfx/help3.lmp", "gfx/help4.lmp", "gfx/help5.lmp",
    };

    /// What the box before the end of the program says, in lines of the
    /// 24 letters the box has room for. The original has eight texts of
    /// its own and shows one by chance; this one is ours.
    constexpr std::array<std::string_view, 4> quit_lines = {
      " Do you really want to  ",
      "    leave the game?     ",
      "                        ",
      "   Y leaves, N stays.   ",
    };

    constexpr std::string_view new_game_question = "Are you sure you want to\nstart a new game?";

    /// One row of the options screen. A slider goes from `least` to
    /// `most` in steps of `step`. With `least` above `most` its number
    /// goes down as the slider goes right. A step of 0 is on or off.
    struct OptionRow
    {
      std::string_view label;
      std::string_view name;
      float least;
      float most;
      float step;
    };

    constexpr std::array<OptionRow, Menu::option_items> option_rows = {{
      {"Screen size", MenuOptions::screen_size_name, 30.0f, 120.0f, 10.0f},
      {"Brightness", MenuOptions::gamma_name, 1.0f, 0.5f, 0.05f},
      {"Mouse Speed", MenuOptions::mouse_speed_name, 1.0f, 11.0f, 0.5f},
      {"Music Volume", MenuOptions::music_volume_name, 0.0f, 1.0f, 0.1f},
      {"Sound Volume", MenuOptions::sound_volume_name, 0.0f, 1.0f, 0.1f},
      {"Always Run", MenuOptions::always_run_name, 0.0f, 1.0f, 0.0f},
      {"Invert Mouse", MenuOptions::invert_mouse_name, 0.0f, 1.0f, 0.0f},
      {"Stick Speed", MenuOptions::stick_speed_name, 1.0f, 11.0f, 0.5f},
    }};

    /// What the option of a row is now, with 1 and 0 for on and off.
    float value_of(const std::size_t row, const MenuOptions &options)
    {
      switch (row)
      {
      case 0: return options.screen_size;
      case 1: return options.gamma;
      case 2: return options.mouse_speed;
      case 3: return options.music_volume;
      case 4: return options.sound_volume;
      case 5: return options.always_run ? 1.0f : 0.0f;
      case 6: return options.invert_mouse ? 1.0f : 0.0f;
      default: return options.stick_speed;
      }
    }

    /// How many times something that happens `rate` times a second has
    /// happened by a time. None for a time before the start, or one that
    /// is no number.
    std::int64_t count_ticks(const double time, const double rate)
    {
      const double ticks = std::floor(time * rate);
      if (!(ticks > 0.0) || ticks > 1.0e15) { return 0; }

      return static_cast<std::int64_t>(ticks);
    }

    /// The cursor of a screen of text, which blinks.
    void add_text_cursor(
      std::vector<HudPicture> &pictures,
      const std::int32_t x,
      const std::int32_t row,
      const double time)
    {
      pictures.push_back({
        .kind = HudPictureKind::Character,
        .name = HudPicture::characters_name,
        .character = text_cursor + static_cast<std::int32_t>(count_ticks(time, 4.0) & 1),
        .x = x,
        .y = first_row_y + row * line_height,
        .anchor = anchor,
      });
    }

    void add_letter(
      std::vector<HudPicture> &pictures,
      const std::int32_t character,
      const std::int32_t x,
      const std::int32_t y)
    {
      pictures.push_back({
        .kind = HudPictureKind::Character,
        .name = HudPicture::characters_name,
        .character = character,
        .x = x,
        .y = y,
        .anchor = anchor,
      });
    }

    /// A line of text in bronze, as the menus write all but a few of
    /// their words. A space takes its room and is not drawn.
    void add_bronze_line(
      std::vector<HudPicture> &pictures,
      const std::string_view text,
      const std::int32_t x,
      const std::int32_t y)
    {
      std::int32_t at = x;
      for (const char letter : text)
      {
        const std::int32_t character = static_cast<unsigned char>(letter);
        if (character != ' ')
        {
          add_letter(pictures, character < HudText::bronze ? character + HudText::bronze : character, at, y);
        }
        at += HudPicture::character_size;
      }
    }

    /// The places the slider of the frame limit has: one for every ten
    /// frames a second from the least to the most, and one more at its
    /// right end for no limit.
    constexpr std::int32_t frame_limit_places =
      static_cast<std::int32_t>(
        (MenuOptions::most_frame_limit - MenuOptions::least_frame_limit) / MenuOptions::frame_limit_step) + 2;

    /// The place of a frame limit on its slider. No limit, 0 and below, is
    /// the last.
    std::int32_t frame_limit_place(const float limit)
    {
      if (!(limit > 0.0f)) { return frame_limit_places - 1; }

      const float held = std::clamp(limit, MenuOptions::least_frame_limit, MenuOptions::most_frame_limit);
      return static_cast<std::int32_t>(
        std::lround((held - MenuOptions::least_frame_limit) / MenuOptions::frame_limit_step));
    }

    /// The frame limit at a place of its slider, which stops at its ends.
    float frame_limit_at(const std::int32_t place)
    {
      const std::int32_t held = std::clamp(place, 0, frame_limit_places - 1);
      if (held == frame_limit_places - 1) { return 0.0f; }

      return MenuOptions::least_frame_limit + static_cast<float>(held) * MenuOptions::frame_limit_step;
    }

    /// A slider with its knob at a part of its way, from 0 to 1.
    void add_slider(std::vector<HudPicture> &pictures, const std::int32_t x, const std::int32_t y, float part)
    {
      // a part that is no number is the left end
      if (!(part > 0.0f)) { part = 0.0f; }
      if (part > 1.0f) { part = 1.0f; }

      add_letter(pictures, slider_left, x - 8, y);
      for (std::int32_t i = 0; i < slider_length; i++) { add_letter(pictures, slider_middle, x + i * 8, y); }
      add_letter(pictures, slider_right, x + slider_length * 8, y);

      const auto way = static_cast<float>((slider_length - 1) * 8);
      add_letter(pictures, slider_knob, x + static_cast<std::int32_t>(way * part), y);
    }

    std::int32_t wrap(const std::int32_t cursor, const std::int32_t count)
    {
      if (cursor < 0) { return count - 1; }

      return cursor >= count ? 0 : cursor;
    }

    MenuAction sound(const std::string_view name)
    {
      return {.kind = MenuActionKind::PlaySound, .name = name};
    }
  }

  std::vector<MenuAction> Menu::Open()
  {
    std::vector<MenuAction> actions;
    if (!IsOpen()) { Enter(MenuScreen::Main, actions); }
    return actions;
  }

  void Menu::Close()
  {
    _screen = MenuScreen::None;
  }

  bool Menu::IsOpen() const
  {
    return _screen != MenuScreen::None;
  }

  MenuScreen Menu::GetScreen() const
  {
    return _screen;
  }

  std::int32_t Menu::GetCursor() const
  {
    switch (_screen)
    {
    case MenuScreen::Main: return _main_cursor;
    case MenuScreen::SinglePlayer: return _single_player_cursor;
    case MenuScreen::Load:
    case MenuScreen::Save: return _slot_cursor;
    case MenuScreen::Options: return _options_cursor;
    case MenuScreen::Video: return _video_cursor;
    case MenuScreen::Help: return _help_page;
    default: return 0;
    }
  }

  std::vector<MenuAction> Menu::Press(const MenuKey key, const MenuOptions &options, const MenuGame &game)
  {
    switch (_screen)
    {
    case MenuScreen::Main: return PressOnMain(key);
    case MenuScreen::SinglePlayer: return PressOnSinglePlayer(key, game);
    case MenuScreen::NewGame: return PressOnNewGame(key);
    case MenuScreen::Load:
    case MenuScreen::Save: return PressOnSlots(key, game);
    case MenuScreen::Options: return PressOnOptions(key, options);
    case MenuScreen::Video: return PressOnVideo(key, options, game);
    case MenuScreen::Help: return PressOnHelp(key);
    case MenuScreen::Quit: return PressOnQuit(key);
    default: return {};
    }
  }

  void Menu::Enter(const MenuScreen screen, std::vector<MenuAction> &actions)
  {
    _screen = screen;
    actions.push_back(sound(MenuAction::enter_sound));
  }

  std::vector<MenuAction> Menu::PressOnMain(const MenuKey key)
  {
    std::vector<MenuAction> actions;
    switch (key)
    {
    case MenuKey::Back:
      Close();
      actions.push_back({.kind = MenuActionKind::Resume});
      break;

    case MenuKey::Down:
      _main_cursor = wrap(_main_cursor + 1, main_items);
      actions.push_back(sound(MenuAction::move_sound));
      break;

    case MenuKey::Up:
      _main_cursor = wrap(_main_cursor - 1, main_items);
      actions.push_back(sound(MenuAction::move_sound));
      break;

    case MenuKey::Select:
    {
      constexpr std::array<MenuScreen, main_items> screens = {
        MenuScreen::SinglePlayer, MenuScreen::Options, MenuScreen::Help, MenuScreen::Quit,
      };
      const MenuScreen screen = screens[static_cast<std::size_t>(_main_cursor)];

      // the help starts at its first page each time
      if (screen == MenuScreen::Help) { _help_page = 0; }
      Enter(screen, actions);
      break;
    }

    default:
      break;
    }
    return actions;
  }

  std::vector<MenuAction> Menu::PressOnSinglePlayer(const MenuKey key, const MenuGame &game)
  {
    std::vector<MenuAction> actions;
    switch (key)
    {
    case MenuKey::Back:
      Enter(MenuScreen::Main, actions);
      break;

    case MenuKey::Down:
      _single_player_cursor = wrap(_single_player_cursor + 1, single_player_items);
      actions.push_back(sound(MenuAction::move_sound));
      break;

    case MenuKey::Up:
      _single_player_cursor = wrap(_single_player_cursor - 1, single_player_items);
      actions.push_back(sound(MenuAction::move_sound));
      break;

    case MenuKey::Select:
      // the sound comes whatever the item does, as in the original
      actions.push_back(sound(MenuAction::enter_sound));
      if (_single_player_cursor == 0)
      {
        if (game.is_running) { _screen = MenuScreen::NewGame; }
        else
        {
          Close();
          actions.push_back({.kind = MenuActionKind::NewGame});
        }
      }
      else if (_single_player_cursor == 1) { _screen = MenuScreen::Load; }
      else if (game.is_running && !game.is_in_intermission) { _screen = MenuScreen::Save; }
      break;

    default:
      break;
    }
    return actions;
  }

  std::vector<MenuAction> Menu::PressOnNewGame(const MenuKey key)
  {
    std::vector<MenuAction> actions;
    if (key == MenuKey::Yes || key == MenuKey::Select)
    {
      Close();
      actions.push_back({.kind = MenuActionKind::NewGame});
    }
    else if (key == MenuKey::No || key == MenuKey::Back) { _screen = MenuScreen::SinglePlayer; }
    return actions;
  }

  std::vector<MenuAction> Menu::PressOnSlots(const MenuKey key, const MenuGame &game)
  {
    constexpr auto slots = static_cast<std::int32_t>(MenuGame::slot_count);

    std::vector<MenuAction> actions;
    switch (key)
    {
    case MenuKey::Back:
      Enter(MenuScreen::SinglePlayer, actions);
      break;

    case MenuKey::Up:
    case MenuKey::Left:
      _slot_cursor = wrap(_slot_cursor - 1, slots);
      actions.push_back(sound(MenuAction::move_sound));
      break;

    case MenuKey::Down:
    case MenuKey::Right:
      _slot_cursor = wrap(_slot_cursor + 1, slots);
      actions.push_back(sound(MenuAction::move_sound));
      break;

    case MenuKey::Select:
      if (_screen == MenuScreen::Save)
      {
        Close();
        actions.push_back({.kind = MenuActionKind::SaveGame, .slot = _slot_cursor});
        break;
      }

      // a slot with nothing in it gives the sound and no more
      actions.push_back(sound(MenuAction::enter_sound));
      if (!game.slots[static_cast<std::size_t>(_slot_cursor)].empty())
      {
        Close();
        actions.push_back({.kind = MenuActionKind::LoadGame, .slot = _slot_cursor});
      }
      break;

    default:
      break;
    }
    return actions;
  }

  std::vector<MenuAction> Menu::PressOnOptions(const MenuKey key, const MenuOptions &options)
  {
    std::vector<MenuAction> actions;
    switch (key)
    {
    case MenuKey::Back:
      Enter(MenuScreen::Main, actions);
      break;

    case MenuKey::Up:
      _options_cursor = wrap(_options_cursor - 1, option_rows_shown);
      actions.push_back(sound(MenuAction::move_sound));
      break;

    case MenuKey::Down:
      _options_cursor = wrap(_options_cursor + 1, option_rows_shown);
      actions.push_back(sound(MenuAction::move_sound));
      break;

    case MenuKey::Left:
      if (_options_cursor < option_items) { ChangeOption(-1, options, actions); }
      break;

    case MenuKey::Right:
      if (_options_cursor < option_items) { ChangeOption(1, options, actions); }
      break;

    case MenuKey::Select:
      // the last item leads to the video settings
      if (_options_cursor == option_items)
      {
        Enter(MenuScreen::Video, actions);
        break;
      }

      // enter is a step to the right, and gives its own sound after
      ChangeOption(1, options, actions);
      actions.push_back(sound(MenuAction::enter_sound));
      break;

    default:
      break;
    }
    return actions;
  }

  std::vector<MenuAction> Menu::PressOnVideo(const MenuKey key, const MenuOptions &options, const MenuGame &game)
  {
    std::vector<MenuAction> actions;
    const auto set = [&actions](const std::string_view name, const float value)
    {
      actions.push_back({.kind = MenuActionKind::SetOption, .name = name, .value = value});
    };

    // a step to the right or to the left, as an arrow and enter ask
    const auto change = [&](const std::int32_t direction)
    {
      // the sound comes also when nothing can change, as with a slider at
      // its end
      actions.push_back(sound(MenuAction::change_sound));

      switch (_video_cursor)
      {
      case 0:
      {
        // window, without borders, the whole display, and around
        const auto mode = static_cast<std::int32_t>(std::clamp(options.window_mode, 0.0f, 2.0f));
        set(MenuOptions::window_mode_name, static_cast<float>(wrap(mode + direction, 3)));
        break;
      }
      case 1:
      {
        // the sizes of the display, which come the largest first: to the
        // right is larger
        const std::vector<MenuSize> &sizes = game.display_sizes;
        if (sizes.empty()) { break; }

        const MenuSize now{
          static_cast<std::int32_t>(options.window_width), static_cast<std::int32_t>(options.window_height)};
        const auto found = std::ranges::find(sizes, now);

        // a size the display does not offer goes to the largest
        std::size_t index = 0;
        if (found != sizes.end())
        {
          const auto at = static_cast<std::int32_t>(found - sizes.begin());
          index = static_cast<std::size_t>(std::clamp(at - direction, 0, static_cast<std::int32_t>(sizes.size()) - 1));
        }
        set(MenuOptions::window_width_name, static_cast<float>(sizes[index].width));
        set(MenuOptions::window_height_name, static_cast<float>(sizes[index].height));
        break;
      }
      case 2:
        set(MenuOptions::vertical_sync_name, options.vertical_sync ? 0.0f : 1.0f);
        break;
      default:
        set(MenuOptions::frame_limit_name, frame_limit_at(frame_limit_place(options.frame_limit) + direction));
        break;
      }
    };

    switch (key)
    {
    case MenuKey::Back:
      Enter(MenuScreen::Options, actions);
      break;

    case MenuKey::Up:
      _video_cursor = wrap(_video_cursor - 1, video_items);
      actions.push_back(sound(MenuAction::move_sound));
      break;

    case MenuKey::Down:
      _video_cursor = wrap(_video_cursor + 1, video_items);
      actions.push_back(sound(MenuAction::move_sound));
      break;

    case MenuKey::Left:
      change(-1);
      break;

    case MenuKey::Right:
      change(1);
      break;

    case MenuKey::Select:
      change(1);
      actions.push_back(sound(MenuAction::enter_sound));
      break;

    default:
      break;
    }
    return actions;
  }

  std::vector<MenuAction> Menu::PressOnHelp(const MenuKey key)
  {
    std::vector<MenuAction> actions;
    switch (key)
    {
    case MenuKey::Back:
      Enter(MenuScreen::Main, actions);
      break;

    case MenuKey::Up:
    case MenuKey::Right:
      _help_page = wrap(_help_page + 1, help_pages);
      actions.push_back(sound(MenuAction::enter_sound));
      break;

    case MenuKey::Down:
    case MenuKey::Left:
      _help_page = wrap(_help_page - 1, help_pages);
      actions.push_back(sound(MenuAction::enter_sound));
      break;

    default:
      break;
    }
    return actions;
  }

  std::vector<MenuAction> Menu::PressOnQuit(const MenuKey key)
  {
    std::vector<MenuAction> actions;
    if (key == MenuKey::Yes || key == MenuKey::Select) { actions.push_back({.kind = MenuActionKind::Quit}); }
    else if (key == MenuKey::No || key == MenuKey::Back) { Enter(MenuScreen::Main, actions); }
    return actions;
  }

  void Menu::ChangeOption(
    const std::int32_t direction,
    const MenuOptions &options,
    std::vector<MenuAction> &actions) const
  {
    const auto index = static_cast<std::size_t>(_options_cursor);
    const OptionRow &row = option_rows[index];
    const float now = value_of(index, options);

    float value;
    if (row.step == 0.0f) { value = now != 0.0f ? 0.0f : 1.0f; }
    else
    {
      // a slider whose number goes down to the right steps the other way
      const double way = row.least <= row.most ? 1.0 : -1.0;
      const double low = std::min(row.least, row.most);
      const double high = std::max(row.least, row.most);

      // rounded, so that steps that are no whole number in a float still
      // reach the ends. What is no number becomes the low end.
      double stepped = static_cast<double>(now) + way * direction * static_cast<double>(row.step);
      stepped = std::round(stepped * 10000.0) / 10000.0;
      if (!(stepped > low)) { stepped = low; }
      if (stepped > high) { stepped = high; }
      value = static_cast<float>(stepped);
    }

    // the sound comes also when the slider is at its end, as in the
    // original
    actions.push_back(sound(MenuAction::change_sound));
    actions.push_back({.kind = MenuActionKind::SetOption, .name = row.name, .value = value});
  }

  std::vector<HudPicture> Menu::Layout(
    const double time,
    const MenuOptions &options,
    const MenuGame &game,
    const MenuTitleWidths &widths) const
  {
    std::vector<HudPicture> pictures;
    switch (_screen)
    {
    case MenuScreen::None:
      break;

    case MenuScreen::Main:
      AddMain(pictures, widths.main, _main_cursor, time);
      break;

    case MenuScreen::SinglePlayer:
      AddPictureScreen(
        pictures, "gfx/ttl_sgl.lmp", widths.single_player, "gfx/sp_menu.lmp", _single_player_cursor, time);
      break;

    case MenuScreen::NewGame:
      // the question alone, over the view, as the original shows it
      return CenterText::LayoutLetters(new_game_question, new_game_question.size());

    case MenuScreen::Load:
      AddSlots(pictures, "gfx/p_load.lmp", widths.load, game, time);
      break;

    case MenuScreen::Save:
      AddSlots(pictures, "gfx/p_save.lmp", widths.save, game, time);
      break;

    case MenuScreen::Options:
      AddOptions(pictures, options, widths.options, time);
      break;

    case MenuScreen::Video:
      AddVideo(pictures, options, widths.video, time);
      break;

    case MenuScreen::Help:
      pictures.push_back({.name = help_names[static_cast<std::size_t>(_help_page)], .x = 0, .y = 0, .anchor = anchor});
      break;

    case MenuScreen::Quit:
      // over the main menu, which it came from
      AddMain(pictures, widths.main, _main_cursor, time);
      AddTextBox(pictures, 56, 76, 24, 4);
      for (std::size_t i = 0; i < quit_lines.size(); i++)
      {
        add_bronze_line(pictures, quit_lines[i], 64, 84 + static_cast<std::int32_t>(i) * line_height);
      }
      break;
    }
    return pictures;
  }

  void Menu::AddTextBox(
    std::vector<HudPicture> &pictures,
    const std::int32_t x,
    const std::int32_t y,
    const std::int32_t width,
    const std::int32_t lines)
  {
    const std::int32_t bottom = y + (std::max(lines, 0) + 1) * 8;

    // one column of the box: its top, a piece for each line, its bottom.
    const auto add_column = [&](
      const std::int32_t at,
      const std::string_view top,
      const std::string_view middle,
      const std::string_view second_middle,
      const std::string_view low)
    {
      pictures.push_back({.name = top, .x = at, .y = y, .anchor = anchor});
      for (std::int32_t line = 0; line < lines; line++)
      {
        // the middle has another piece from its second line on
        pictures.push_back({
          .name = line == 0 ? middle : second_middle,
          .x = at,
          .y = y + (line + 1) * 8,
          .anchor = anchor,
        });
      }
      pictures.push_back({.name = low, .x = at, .y = bottom, .anchor = anchor});
    };

    add_column(x, "gfx/box_tl.lmp", "gfx/box_ml.lmp", "gfx/box_ml.lmp", "gfx/box_bl.lmp");

    std::int32_t at = x + 8;
    for (std::int32_t left = width; left > 0; left -= 2)
    {
      add_column(at, "gfx/box_tm.lmp", "gfx/box_mm.lmp", "gfx/box_mm2.lmp", "gfx/box_bm.lmp");
      at += 16;
    }

    add_column(at, "gfx/box_tr.lmp", "gfx/box_mr.lmp", "gfx/box_mr.lmp", "gfx/box_br.lmp");
  }

  std::vector<HudPicture> Menu::LayoutPause(const std::int32_t width, const std::int32_t height)
  {
    return {{.name = "gfx/pause.lmp", .x = (screen_width - width) / 2, .y = (200 - 48 - height) / 2, .anchor = anchor}};
  }

  void Menu::AddMain(
    std::vector<HudPicture> &pictures,
    const std::int32_t title_width,
    const std::int32_t cursor,
    const double time)
  {
    static constexpr std::string_view items = "gfx/mainmenu.lmp";

    pictures.push_back({.name = "gfx/qplaque.lmp", .x = 16, .y = title_y, .anchor = anchor});
    pictures.push_back({
      .name = "gfx/ttl_main.lmp",
      .x = (screen_width - title_width) / 2,
      .y = title_y,
      .anchor = anchor,
    });

    // The picture has the five items of the original, each as high as an
    // item. The second, multiplayer, is left out (#65): the first is drawn,
    // and what comes after the second where the second was.
    pictures.push_back({.name = items, .x = 72, .y = first_row_y, .anchor = anchor, .rows = item_height});
    pictures.push_back({
      .name = items,
      .x = 72,
      .y = first_row_y + item_height,
      .anchor = anchor,
      .first_row = 2 * item_height,
    });

    const auto frame = static_cast<std::size_t>(count_ticks(time, 10.0) % 6);
    pictures.push_back({
      .name = cursor_names[frame],
      .x = 54,
      .y = first_row_y + cursor * item_height,
      .anchor = anchor,
    });
  }

  void Menu::AddPictureScreen(
    std::vector<HudPicture> &pictures,
    const std::string_view title,
    const std::int32_t title_width,
    const std::string_view items,
    const std::int32_t cursor,
    const double time)
  {
    pictures.push_back({.name = "gfx/qplaque.lmp", .x = 16, .y = title_y, .anchor = anchor});
    pictures.push_back({.name = title, .x = (screen_width - title_width) / 2, .y = title_y, .anchor = anchor});
    pictures.push_back({.name = items, .x = 72, .y = first_row_y, .anchor = anchor});

    const auto frame = static_cast<std::size_t>(count_ticks(time, 10.0) % 6);
    pictures.push_back({
      .name = cursor_names[frame],
      .x = 54,
      .y = first_row_y + cursor * item_height,
      .anchor = anchor,
    });
  }

  void Menu::AddSlots(
    std::vector<HudPicture> &pictures,
    const std::string_view title,
    const std::int32_t title_width,
    const MenuGame &game,
    const double time) const
  {
    pictures.push_back({.name = title, .x = (screen_width - title_width) / 2, .y = title_y, .anchor = anchor});

    for (std::size_t i = 0; i < MenuGame::slot_count; i++)
    {
      const std::string_view text = game.slots[i].empty() ? unused_slot : std::string_view(game.slots[i]);
      add_bronze_line(
        pictures, text.substr(0, slot_letters), 16, first_row_y + static_cast<std::int32_t>(i) * line_height);
    }

    add_text_cursor(pictures, 8, _slot_cursor, time);
  }

  void Menu::AddOptions(
    std::vector<HudPicture> &pictures,
    const MenuOptions &options,
    const std::int32_t title_width,
    const double time) const
  {
    pictures.push_back({.name = "gfx/qplaque.lmp", .x = 16, .y = title_y, .anchor = anchor});
    pictures.push_back({
      .name = "gfx/p_option.lmp",
      .x = (screen_width - title_width) / 2,
      .y = title_y,
      .anchor = anchor,
    });

    for (std::size_t i = 0; i < option_rows.size(); i++)
    {
      const OptionRow &row = option_rows[i];
      const std::int32_t y = first_row_y + static_cast<std::int32_t>(i) * line_height;
      const float value = value_of(i, options);

      add_bronze_line(pictures, row.label, option_label_end - static_cast<std::int32_t>(row.label.size()) * 8, y);
      if (row.step == 0.0f) { add_bronze_line(pictures, value != 0.0f ? "on" : "off", option_value_x, y); }
      else { add_slider(pictures, option_value_x, y, (value - row.least) / (row.most - row.least)); }
    }

    // the item that leads to the video settings, which has no value
    add_bronze_line(
      pictures, video_label, option_label_end - static_cast<std::int32_t>(video_label.size()) * 8,
      first_row_y + option_items * line_height);

    add_text_cursor(pictures, option_cursor_x, _options_cursor, time);
  }

  void Menu::AddVideo(
    std::vector<HudPicture> &pictures,
    const MenuOptions &options,
    const std::int32_t title_width,
    const double time) const
  {
    pictures.push_back({.name = "gfx/qplaque.lmp", .x = 16, .y = title_y, .anchor = anchor});
    pictures.push_back({
      .name = "gfx/vidmodes.lmp",
      .x = (screen_width - title_width) / 2,
      .y = title_y,
      .anchor = anchor,
    });

    constexpr std::array<std::string_view, 3> modes = {"window", "borderless", "fullscreen"};
    const auto mode = static_cast<std::size_t>(std::clamp(options.window_mode, 0.0f, 2.0f));

    // the size as `1920x1080`, or that it is as the game was started
    const auto width = static_cast<std::int32_t>(options.window_width);
    const auto height = static_cast<std::int32_t>(options.window_height);
    const std::string size = width > 0 && height > 0 ? std::to_string(width) + "x" + std::to_string(height) : "as started";

    // the limit is said with its name, since its slider is where the
    // words of the others are
    const std::int32_t place = frame_limit_place(options.frame_limit);
    const std::string limit = "Max FPS " + (place == frame_limit_places - 1
                                              ? std::string("unlimited")
                                              : std::to_string(static_cast<std::int32_t>(frame_limit_at(place))));

    const std::array<std::pair<std::string_view, std::string_view>, video_items> rows = {{
      {"Video Mode", modes[mode]},
      {"Resolution", size},
      {"Vertical Sync", options.vertical_sync ? "on" : "off"},
      {limit, ""},
    }};
    for (std::size_t i = 0; i < rows.size(); i++)
    {
      const std::int32_t y = first_row_y + static_cast<std::int32_t>(i) * line_height;
      add_bronze_line(pictures, rows[i].first, option_label_end - static_cast<std::int32_t>(rows[i].first.size()) * 8, y);
      if (!rows[i].second.empty()) { add_bronze_line(pictures, rows[i].second, option_value_x, y); }
      else
      {
        add_slider(pictures, option_value_x, y, static_cast<float>(place) / static_cast<float>(frame_limit_places - 1));
      }
    }

    add_text_cursor(pictures, option_cursor_x, _video_cursor, time);
  }
} // quake
