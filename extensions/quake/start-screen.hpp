#ifndef QUAKE_START_SCREEN_HPP
#define QUAKE_START_SCREEN_HPP

#include <string>
#include <string_view>
#include <vector>

#include <neon/extension/neon-extension.hpp>

#include "game/basedir.hpp"

namespace quake
{
  /// What is shown before the game starts, when it cannot start at once:
  /// the menu that asks which copy of the data to play, or why the game
  /// cannot start at all, with a button that closes it. Both are files of
  /// the user interface of the engine, used with a mouse, the keyboard, or
  /// a controller.
  class StartScreen
  {
    std::vector<neon::extension::UiListener> _listening;

    // the copy that was picked and not yet taken, or -1
    int _picked = -1;

    bool _asks = false;

  public:
    static constexpr std::string_view picker_file = "assets://ui/basedirs.ui.yml";
    static constexpr std::string_view problem_file = "assets://ui/cannot-start.ui.yml";

    /// Asks which of the copies to play, with a button for each, the first
    /// with the focus.
    void ShowPicker(neon::extension::World &world, const std::vector<Basedir> &basedirs);

    /// Says why the game cannot start, and closes the application when its
    /// button is chosen.
    void ShowProblem(neon::extension::World &world, const std::string &reason);

    /// The copy that was picked since the last call, as its index, or -1.
    /// Once one is picked, the menu is closed here, outside of what the user
    /// interface was doing when it told of the click.
    [[nodiscard]] int TakePicked(neon::extension::World &world);
  };
} // quake

#endif //QUAKE_START_SCREEN_HPP
