#ifndef QUAKE_MENU_HPP
#define QUAKE_MENU_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "hud-picture.hpp"
#include "menu-action.hpp"
#include "menu-game.hpp"
#include "menu-key.hpp"
#include "menu-options.hpp"
#include "menu-screen.hpp"
#include "menu-title-widths.hpp"

namespace quake
{
  /// The menus of the original: which screen is open, where the cursor
  /// is on each, and what a key does. They draw nothing and read no key.
  /// The host hands them what was pressed and is told what to do, and
  /// asks them what to draw.
  ///
  /// The screens are pictures of the paks and letters of the console at
  /// the places of the original, in the screen of 320 by 200 that
  /// `HudPicture` describes, with `HudAnchor::Center`. The original put
  /// them at the top of a higher screen. Under them it made the view
  /// darker, which a `HudPicture` cannot say: the host does that for as
  /// long as IsOpen() says so.
  ///
  /// Left out of the original are the screens for the keys and the video
  /// modes, the console, and all of multiplayer, which is not there yet
  /// (#65): the main menu is drawn without its item.
  ///
  /// ```
  /// Menu menu;
  /// // when escape is pressed while playing
  /// Act(menu.Open());
  /// // every frame
  /// for (const MenuKey key : pressed) { Act(menu.Press(key, options, game)); }
  /// if (menu.IsOpen()) { Draw(menu.Layout(seconds, options, game, widths)); }
  /// ```
  class Menu final
  {
  public:
    /// How many items each screen has, and how many pages the help.
    static constexpr std::int32_t main_items = 4;
    static constexpr std::int32_t single_player_items = 3;
    /// The settings, and after them the item that leads to the video
    /// settings.
    static constexpr std::int32_t option_items = 8;
    static constexpr std::int32_t option_rows_shown = option_items + 1;

    /// The mode of the window, its size, vertical sync, the most frames a
    /// second, and whether there is no limit to them.
    static constexpr std::int32_t video_items = 5;

    /// Which of them the size of the window is, and which the slider of
    /// the frame limit. Each is greyed out and passed over by the cursor
    /// while it changes nothing: the size while the window has no borders
    /// and covers its display whatever its size, the slider while there is
    /// no limit.
    static constexpr std::int32_t window_size_item = 1;
    static constexpr std::int32_t frame_limit_item = 3;
    static constexpr std::int32_t help_pages = 6;

    /// How many letters of what a saved game says about itself are
    /// shown: as many as there is room for to the right edge.
    static constexpr std::size_t slot_letters = 38;

    /// What a slot with no saved game shows.
    static constexpr std::string_view unused_slot = "--- UNUSED SLOT ---";

    /// How wide and high `gfx/pause.lmp` of the original is.
    static constexpr std::int32_t pause_width = 128;
    static constexpr std::int32_t pause_height = 24;

    /// Opens the main menu, as escape does while playing, with the cursor
    /// where it was left. Gives the sound of it. An open menu stays as it
    /// is, and nothing is given.
    std::vector<MenuAction> Open();

    /// Closes the menus, whichever screen is open. The cursors stay.
    void Close();

    [[nodiscard]] bool IsOpen() const;

    [[nodiscard]] MenuScreen GetScreen() const;

    /// Where the cursor of the open screen is, from 0. On the help it is
    /// the page, and 0 on a screen with no cursor.
    [[nodiscard]] std::int32_t GetCursor() const;

    /// What a key does: it moves the cursor, which goes around at both
    /// ends, goes into a screen or back out of it, or changes an option.
    /// Gives what the host must do for it, sounds among it, in the order
    /// to do it in. Nothing while the menus are closed.
    ///
    /// The menus close themselves when a game is started, loaded, or
    /// saved, and when `Back` is pressed on the main menu, which gives
    /// `Resume`.
    ///
    /// `options` is what the options are now: a slider moves one step
    /// from there. `game` says whether a game runs and which slots hold
    /// a saved game.
    std::vector<MenuAction> Press(MenuKey key, const MenuOptions &options = {}, const MenuGame &game = {});

    /// The pictures and letters of the open screen, in the order to draw
    /// them in. Nothing while the menus are closed.
    ///
    /// `time` is in seconds and only has to go on: the cursor of the
    /// screens of pictures turns with it, six pictures at ten a second,
    /// and the one of the screens of text blinks, four times a second.
    [[nodiscard]] std::vector<HudPicture> Layout(
      double time,
      const MenuOptions &options = {},
      const MenuGame &game = {},
      const MenuTitleWidths &widths = {}) const;

    /// Adds the box the original puts under a text, made of the pictures
    /// `gfx/box_tl.lmp` and the like: its upper left corner at x, y, and
    /// inside it room for `lines` lines of `width` letters, which start 8
    /// to the right and 8 down. The middle is made of pieces two letters
    /// wide, so an odd width gets one letter more.
    static void AddTextBox(
      std::vector<HudPicture> &pictures,
      std::int32_t x,
      std::int32_t y,
      std::int32_t width,
      std::int32_t lines);

    /// The picture `gfx/pause.lmp` where the original shows it while the
    /// game is paused: around the middle from left to right, and a little
    /// above the middle. A host that knows the real size hands it in.
    /// The hints below a menu: the button that chooses and the button that
    /// goes back, as pictures of what the player holds or, on the keyboard,
    /// the names of the keys, each with its word. Below a question they
    /// answer yes and no, and the keyboard names Y and N. Nothing while the
    /// menus are closed, and on the pages of help.
    [[nodiscard]] std::vector<HudPicture> LayoutHints(PromptDevice device) const;

    [[nodiscard]] static std::vector<HudPicture> LayoutPause(
      std::int32_t width = pause_width,
      std::int32_t height = pause_height);

  private:
    MenuScreen _screen = MenuScreen::None;

    /// The cursor of each screen, which stays when the screen is left.
    /// The screens to load and to save share one, as in the original.
    std::int32_t _main_cursor = 0;
    std::int32_t _single_player_cursor = 0;
    std::int32_t _slot_cursor = 0;
    std::int32_t _options_cursor = 0;
    std::int32_t _video_cursor = 0;
    std::int32_t _help_page = 0;

    /// Goes to a screen, with the sound of it.
    void Enter(MenuScreen screen, std::vector<MenuAction> &actions);

    std::vector<MenuAction> PressOnMain(MenuKey key);
    std::vector<MenuAction> PressOnSinglePlayer(MenuKey key, const MenuGame &game);
    std::vector<MenuAction> PressOnNewGame(MenuKey key);
    std::vector<MenuAction> PressOnSlots(MenuKey key, const MenuGame &game);
    std::vector<MenuAction> PressOnOptions(MenuKey key, const MenuOptions &options);
    std::vector<MenuAction> PressOnVideo(MenuKey key, const MenuOptions &options, const MenuGame &game);

    /// Whether a video setting changes anything as the options are, see
    /// `window_size_item`.
    [[nodiscard]] static bool IsVideoItemOn(std::int32_t item, const MenuOptions &options);
    std::vector<MenuAction> PressOnHelp(MenuKey key);
    std::vector<MenuAction> PressOnQuit(MenuKey key);

    /// The option under the cursor, one step up or down, or turned over
    /// when it is on or off.
    void ChangeOption(std::int32_t direction, const MenuOptions &options, std::vector<MenuAction> &actions) const;

    /// The main menu: the plaque, the title, the picture of its items
    /// without the one of multiplayer, and the cursor. The picture holds
    /// all five, so it is drawn as two strips, the first item and what
    /// comes after the second, moved up to where the second was.
    static void AddMain(std::vector<HudPicture> &pictures, std::int32_t title_width, std::int32_t cursor, double time);

    /// A screen of the kind of the main menu: the plaque on the left, the
    /// title on top, the picture of the items, and the cursor that turns
    /// next to one of them.
    static void AddPictureScreen(
      std::vector<HudPicture> &pictures,
      std::string_view title,
      std::int32_t title_width,
      std::string_view items,
      std::int32_t cursor,
      double time);

    void AddVideo(
      std::vector<HudPicture> &pictures,
      const MenuOptions &options,
      std::int32_t title_width,
      double time) const;

    void AddSlots(
      std::vector<HudPicture> &pictures,
      std::string_view title,
      std::int32_t title_width,
      const MenuGame &game,
      double time) const;

    void AddOptions(
      std::vector<HudPicture> &pictures,
      const MenuOptions &options,
      std::int32_t title_width,
      double time) const;
  };
} // quake

#endif //QUAKE_MENU_HPP
