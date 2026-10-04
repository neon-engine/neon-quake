#ifndef QUAKE_HUD_VIEW_HPP
#define QUAKE_HUD_VIEW_HPP

#include <map>
#include <string>
#include <vector>

#include <neon/extension/neon-extension.hpp>

#include "formats/wad.hpp"
#include "game-data.hpp"
#include "game/hud-picture.hpp"

namespace quake
{
  /// Draws what the game shows on top of the world: the status bar, the
  /// counts of a level that is over, the words in the middle of the screen,
  /// the menus.
  ///
  /// It is handed a list of pictures with their places on the screen of the
  /// original, 320 by 200, and shows them with the user interface of the
  /// engine: the file `hud.ui.yml`, and in it an image for every picture of
  /// the list, in the order of the list. The pictures are those of
  /// `gfx.wad` and of the `gfx/` folder of the data, handed to the renderer
  /// the first time each is shown. A letter is a picture of its own, cut
  /// out of the sheet of letters.
  ///
  /// The screen counts as 384 pixels of the original high, as the ports of
  /// today draw it, and every picture starts and ends on a whole pixel of
  /// the real screen. An image is only told what changed about it.
  class HudView
  {
    /// A picture that was handed to the renderer: where an image reads it
    /// from, and its size in its own pixels.
    struct Known
    {
      std::string path;
      std::int32_t width = 0;
      std::int32_t height = 0;
    };

    /// An image of the user interface, and where it was last told to be.
    struct Slot
    {
      neon::extension::UiElement element = 0;

      /// Its place among the images, later ones over earlier ones.
      std::int32_t order = -1;

      std::int32_t left = 0;
      std::int32_t top = 0;
      std::int32_t width = 0;
      std::int32_t height = 0;
      bool is_visible = false;
    };

    // the archive of pictures of the status bar, once it was read
    Wad _wad;
    bool _has_wad = false;
    bool _was_read = false;

    // the pictures by their names; one that is not there has no path
    std::map<std::string, Known, std::less<>> _pictures;

    // the letters of the sheet that were cut out, by their number
    std::map<std::int32_t, std::string> _letters;

    // whether the file is shown, and the two elements of it
    bool _is_open = false;
    neon::extension::UiElement _tint = 0;
    neon::extension::UiElement _holder = 0;

    // the colour that lies over the view
    std::string _tint_colour;

    // The images, by the picture they show: as many of each as a list had
    // of it at once. An image keeps its picture and is told its place and
    // its turn among the others, which is less to say than a picture.
    std::map<std::string, std::vector<Slot>, std::less<>> _slots;

    // how many of the images of each picture a list uses, while it is shown
    std::map<std::string, std::size_t, std::less<>> _used;

    // what is shown, and the size of the screen it was laid out for, to
    // tell when it has to be laid out anew
    std::vector<HudPicture> _shown;
    int _view_width = 0;
    int _view_height = 0;

    /// How many pixels of the original the screen is high. The original is
    /// 200; more makes everything smaller, as a larger screen does. It is
    /// what `reference_size` of the file says.
    static constexpr float screen_height = 384.0f;

    /// How many pixels high a screen is for each pixel of the lines at its
    /// top, which are drawn smaller than the rest, as a console is.
    static constexpr float notice_height = 540.0f;

    const Known &Find(const neon::extension::World &world, const GameData &data, std::string_view name);

    const std::string &FindLetter(const neon::extension::World &world, const GameData &data, std::int32_t letter);

    bool Open(const neon::extension::World &world);

  public:
    /// The file of the user interface the pictures are shown in.
    static constexpr const char *file = "extensions://quake/assets/ui/hud.ui.yml";

    /// Shows a list of pictures. An empty list shows nothing.
    void Show(const neon::extension::World &world, const GameData &data, const std::vector<HudPicture> &pictures);

    /// Lays a colour over everything of the world, behind the pictures:
    /// red, green, and blue from 0 to 255 as a screen is given them, and
    /// how much of it, from 0 for none to 1 for nothing else.
    void ShowTint(const neon::extension::World &world, float red, float green, float blue, float amount);

    /// The size of a picture of a name in its own pixels, for laying out
    /// what depends on it. Zero for one that is not there.
    std::int32_t GetWidth(const neon::extension::World &world, const GameData &data, std::string_view name);
  };
} // quake

#endif //QUAKE_HUD_VIEW_HPP
