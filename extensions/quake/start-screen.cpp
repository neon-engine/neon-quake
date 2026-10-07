#include "start-screen.hpp"

namespace quake
{
  using neon::extension::UiElement;
  using neon::extension::UiEvent;
  using neon::extension::World;

  void StartScreen::ShowPicker(World &world, const std::vector<Basedir> &basedirs)
  {
    if (!world.ShowUi(std::string(picker_file)))
    {
      world.Error("The menu that asks which data to play cannot be shown, see the log");
      return;
    }
    _asks = true;

    const UiElement list = world.FindUi("basedirs");
    UiElement first = 0;
    for (std::size_t index = 0; index < basedirs.size(); index++)
    {
      const UiElement button = world.CreateUi(
        "type: button\n"
        "name: basedir-" + std::to_string(index) + "\n"
        "text: \"" + basedirs[index].name + "\"\n"
        "border_radius: 8\n",
        list);
      if (button == 0) { continue; }
      if (first == 0) { first = button; }

      _listening.push_back(world.ListenToUi(button, "click", [this, index](const UiEvent &)
      {
        _picked = static_cast<int>(index);
      }));
    }

    // the buttons came after the file was shown, which gave the focus to
    // nothing, so that a controller could not pick without moving first
    if (first != 0) { world.FocusUi(first); }
  }

  void StartScreen::ShowProblem(World &world, const std::string &reason)
  {
    world.Error("Quake cannot start: " + reason);

    if (!world.ShowUi(std::string(problem_file)))
    {
      // with nothing to show the reason on, the application closes, which
      // is better than a screen that stays black
      world.RequestQuit();
      return;
    }

    world.SetUiField(world.FindUi("reason"), "text", reason);
    _listening.push_back(world.ListenToUi(world.FindUi("exit"), "click", [&world](const UiEvent &)
    {
      world.RequestQuit();
    }));
  }

  int StartScreen::TakePicked(World &world)
  {
    if (!_asks || _picked < 0) { return -1; }

    const int picked = _picked;
    _picked = -1;
    _asks = false;

    for (const auto listening : _listening) { world.UnlistenToUi(listening); }
    _listening.clear();
    world.CloseUi(std::string(picker_file));
    return picked;
  }
} // quake
